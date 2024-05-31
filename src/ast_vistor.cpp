#include "ast.h"
#include "ast_visitor.h"
#include "error.h"
#include "utils.h"
#include <variant>

static std::shared_ptr<Type> _asttype2type(ASTType type) {
    switch (type) {
    case ASTType::INT:
        return std::make_shared<Int32Type>();
    case ASTType::FLOAT:
        return std::make_shared<FloatType>();
    case ASTType::VOID:
        return std::make_shared<VoidType>();
    default:
        return nullptr; // unreachable
    }
}

static ir::Type _type2irtype(std::shared_ptr<Type> type) {
    if (type->is_int32()) {
        return ir::Type::W;
    } else if (type->is_float()) {
        return ir::Type::S;
    } else if (type->is_array() || type->is_pointer()) {
        return ir::Type::L;
    } else {
        return ir::Type::X;
    }
}

void ASTVisitor::visit(const CompUnits &node) {
    for (auto &elm : node) {
        std::visit(overloaded{
                       [this](const Decl &node) { visitDecl(node); },
                       [this](const FuncDef &node) { visitFuncDef(node); },
                   },
                   *elm);
    }
}

void ASTVisitor::visitDecl(const Decl &node) {
    for (auto &elm : *node.var_defs) {
        bool is_const = (node.type == Decl::CONST);
        visitVarDef(*elm, node.btype, is_const);
    }
}

void ASTVisitor::visitVarDef(const VarDef &node, ASTType btype, bool is_const) {
    auto type = visitDims(*node.dims, btype);

    std::shared_ptr<Initializer> initializer = nullptr;
    if (node.init_val != nullptr) {
        initializer = visitInitVal(*node.init_val, type);
    }

    auto symbol = std::make_shared<VariableSymbol>(node.ident, type, is_const,
                                                   initializer);

    if (!_current_scope->add_symbol(symbol)) {
        error(-1, "redefine variable " + node.ident);
    }

    if (is_global_context()) {
        auto elm_type = _asttype2type(btype);
        auto data = ir::Data::create(false, symbol->name, elm_type->get_size(),
                                     _module);
        // TODO: append data items, need ordered map
    } else {
        if (type->is_array()) {
            auto elm_type = _asttype2type(btype);
            auto elm_ir_type = _type2irtype(elm_type);

            symbol->value =
                _builder.create_alloc(elm_ir_type, type->get_size());

            if (symbol->initializer) {
                for (auto &[index, val] : symbol->initializer->get_values()) {
                    auto offset =
                        ir::ConstBits::get(elm_type->get_size() * index);
                    auto elm_addr =
                        _builder.create_add(ir::Type::L, symbol->value, offset);
                    _builder.create_store(elm_ir_type, val, elm_addr);
                }
            }
        } else {
            symbol->value =
                _builder.create_alloc(_type2irtype(type), type->get_size());
            if (symbol->initializer) {
                _builder.create_store(_type2irtype(type),
                                      symbol->initializer->get_values()[0],
                                      symbol->value);
            }
        }
    }
}

std::shared_ptr<Initializer>
ASTVisitor::visitInitVal(const InitVal &node, std::shared_ptr<Type> type) {
    return std::visit(
        overloaded{
            [this](const Exp &node) {
                auto initializer = std::make_shared<Initializer>();
                auto [exp_type, exp_value] = visitConstExp(node);
                initializer->insert(exp_value);
                return initializer;
            },
            [this, &type](const ArrayInitVal &node) {
                if (!type->is_array()) {
                    error(-1, "cannot use array initializer on non-array type");
                    return std::shared_ptr<Initializer>(nullptr);
                }
                auto array_type = std::static_pointer_cast<ArrayType>(type);

                // get element number of array, for example, a[2][3] has 6
                // elements, so its initializer should have 6 elements
                auto initializer = std::make_shared<Initializer>(
                    array_type->get_total_elm_count());
                for (auto &elm : node.items) {
                    // get initializer of each element
                    auto elm_initializer =
                        visitInitVal(*elm, array_type->get_base_type());
                    // then insert the initializer to the array initializer
                    initializer->insert(*elm_initializer);
                }

                return initializer;
            },
        },
        node);
}

std::shared_ptr<Type> ASTVisitor::visitDims(const Dims &node, ASTType btype) {
    auto base_type = _asttype2type(btype);
    TypeBuilder tb(base_type);

    // reverse is needed
    for (auto it = node.rbegin(); it != node.rend(); it++) {
        auto exp = *it;
        if (exp == nullptr) {
            tb.in_ptr();
        } else {
            // TODO: get const value
            tb.in_array(3);
        }
    }

    return tb.get_type();
}

void ASTVisitor::visitFuncDef(const FuncDef &node) {
    // get symbols of params
    auto param_symbols = visitFuncFParams(*node.func_fparams);

    // get function type
    auto return_type = _asttype2type(node.func_type);
    std::vector<std::shared_ptr<Type>> params_type;
    std::vector<ir::Type> params_ir_type;
    for (auto &param_symbol : param_symbols) {
        params_type.push_back(param_symbol->type);
        params_ir_type.push_back(_type2irtype(param_symbol->type));
    }

    // add function symbol
    auto symbol =
        std::make_shared<FunctionSymbol>(node.ident, params_type, return_type);
    if (!_current_scope->add_symbol(symbol)) {
        error(-1, "redefine function " + node.ident);
        return;
    }

    // create function in IR
    auto [ir_func, ir_params] = ir::Function::create(
        symbol->name == "main", symbol->name, _type2irtype(return_type),
        params_ir_type, _module);
    symbol->value = ir_func->get_address();
    _builder.set_function(ir_func);

    // going into funciton scope
    _current_scope = _current_scope->push_scope();

    // add symbols of params to symbol table
    int no = 0;
    for (auto &param_symbol : param_symbols) {
        if (!_current_scope->add_symbol(param_symbol)) {
            error(-1, "redefine parameter " + param_symbol->name);
            no++;
            continue;
        }

        // alloc and store for param values
        auto param_ir_type = _type2irtype(param_symbol->type);
        param_symbol->value = _builder.create_alloc(
            param_ir_type, param_symbol->type->get_size());
        _builder.create_store(param_ir_type, ir_params[no],
                              param_symbol->value);

        no++;
    }

    _builder.create_label("body");

    visitBlockItems(*node.block);

    _current_scope = _current_scope->pop_scope();
}

std::vector<std::shared_ptr<Symbol>>
ASTVisitor::visitFuncFParams(const FuncFParams &node) {
    std::vector<std::shared_ptr<Symbol>> params;

    for (auto &elm : node) {
        auto type = visitDims(*elm->dims, elm->btype);
        auto symbol =
            std::make_shared<VariableSymbol>(elm->ident, type, false, nullptr);
        params.push_back(symbol);
    }

    return params;
}

void ASTVisitor::visitBlockItems(const BlockItems &node) {
    for (auto &elm : node) {
        std::visit(overloaded{
                       [this](const Decl &node) { visitDecl(node); },
                       [this](const Stmt &node) { visitStmt(node); },
                   },
                   *elm);
    }
}

void ASTVisitor::visitStmt(const Stmt &node) {
    std::visit(overloaded{
                   [this](const AssignStmt &node) { visitAssignStmt(node); },
                   [this](const ExpStmt &node) { visitExpStmt(node); },
                   [this](const BlockStmt &node) { visitBlockStmt(node); },
                   [this](const IfStmt &node) { visitIfStmt(node); },
                   [this](const WhileStmt &node) { visitWhileStmt(node); },
                   [this](const ControlStmt &node) { visitControlStmt(node); },
                   [this](const ReturnStmt &node) { visitReturnStmt(node); },
               },
               node);
}

void ASTVisitor::visitAssignStmt(const AssignStmt &node) {
    auto [exp_type, exp_val] = visitExp(*node.exp);
    auto [lval_type, lval_val] = visitLVal(*node.lval);

    // TODO: check if type of exp and lval are same. if not, need type convert
    _builder.create_store(_type2irtype(lval_type), exp_val, lval_val);
}

void ASTVisitor::visitExpStmt(const ExpStmt &node) {
    if (node.exp) {
        visitExp(*node.exp);
    }
}

void ASTVisitor::visitBlockStmt(const BlockStmt &node) {
    _current_scope = _current_scope->push_scope();
    visitBlockItems(*node.block);
    _current_scope = _current_scope->pop_scope();
}

void ASTVisitor::visitIfStmt(const IfStmt &node) {
    // TODO: fill jump targets
    auto [jmp_to_true, jmp_to_false] = visitCond(*node.cond);

    _builder.create_label("if_true");

    _current_scope = _current_scope->push_scope();
    visitStmt(*node.if_stmt);
    _current_scope = _current_scope->pop_scope();

    std::shared_ptr<ir::Block> jmp_to_join;
    if (node.else_stmt) {
        jmp_to_join = _builder.create_jmp(nullptr);
    }

    _builder.create_label("if_false");

    if (node.else_stmt) {
        _current_scope = _current_scope->push_scope();
        visitStmt(*node.else_stmt);
        _current_scope = _current_scope->pop_scope();

        auto join_block = _builder.create_label("if_join");
        jmp_to_join->jump.blk[0] = join_block;
    }
}

void ASTVisitor::visitWhileStmt(const WhileStmt &node) {
    auto cond_block = _builder.create_label("while_cond");

    // TODO: fill jump targets
    auto [jmp_to_true, jmp_to_false] = visitCond(*node.cond);

    _builder.create_label("while_body");

    _current_scope = _current_scope->push_scope();
    visitStmt(*node.stmt);
    _current_scope = _current_scope->pop_scope();

    _builder.create_jmp(cond_block);

    _builder.create_label("while_join");
}

void ASTVisitor::visitControlStmt(const ControlStmt &node) {
    if (node.type == ControlStmt::BREAK) {
        // IR: jmp @while_join
    } else { // CONTINUE
        // IR: jmp @while_cond
    }
}

void ASTVisitor::visitReturnStmt(const ReturnStmt &node) {
    if (node.exp) {
        auto [exp_type, exp_val] = visitExp(*node.exp);
        _builder.create_ret(exp_val);
    }
    _builder.create_ret(nullptr);
}

exp_return_t ASTVisitor::visitConstExp(const Exp &node) {
    // TODO: check if the expression is constant
    if (0) {
        error(-1, "not a const expression");
    }
    return visitExp(node);
}

exp_return_t ASTVisitor::visitExp(const Exp &node) {
    return std::visit(
        overloaded{
            [this](const BinaryExp &node) { return visitBinaryExp(node); },
            [this](const LValExp &node) { return visitLValExp(node); },
            [this](const CallExp &node) { return visitCallExp(node); },
            [this](const UnaryExp &node) { return visitUnaryExp(node); },
            [this](const CompareExp &node) { return visitCompareExp(node); },
            [this](const Number &node) { return visitNumber(node); },
        },
        node);
}

static std::shared_ptr<Type> _calc_type(std::shared_ptr<Type> ltype,
                                        std::shared_ptr<Type> rtype) {
    if (ltype->is_float()) {
        return ltype;
    } else if (ltype->is_int32()) {
        if (rtype->is_float()) {
            return rtype;
        } else if (rtype->is_int32()) {
            return rtype;
        } else {
            return std::make_shared<ErrorType>(); // error
        }
    } else {
        return std::make_shared<ErrorType>(); // error
    }
}

exp_return_t ASTVisitor::visitBinaryExp(const BinaryExp &node) {
    auto [left_type, left_val] = visitExp(*node.left);
    auto [right_type, right_val] = visitExp(*node.right);

    // TODO: type convert and support different ops
    auto type = _calc_type(left_type, right_type);
    auto ir_type = _type2irtype(type);
    auto val = _builder.create_add(ir_type, left_val, right_val);

    return exp_return_t(_calc_type(left_type, right_type), nullptr);
}

exp_return_t ASTVisitor::visitLValExp(const LValExp &node) {
    auto [type, val] = visitLVal(*node.lval);

    // still an array, the value is the address itself
    if (type->is_array()) {
        return exp_return_t(type, val);
    }

    // since lval is in an expression, we need to get the value from the
    // lval address
    auto exp_val = _builder.create_load(_type2irtype(type), val);

    return exp_return_t(type, exp_val);
}

exp_return_t ASTVisitor::visitLVal(const LVal &node) {
    return std::visit(
        overloaded{
            [this](const Ident &node) {
                auto symbol = _current_scope->get_symbol(node);
                if (!symbol) {
                    error(-1, "undefined symbol " + node);
                    return exp_return_t(std::make_shared<VoidType>(), nullptr);
                }

                return exp_return_t(symbol->type, symbol->value);
            },
            [this](const Index &node) {
                auto [lval_type, lval_val] = visitLVal(*node.lval);
                auto [exp_type, exp_val] = visitExp(*node.exp);

                if (!lval_type->is_array() && !lval_type->is_pointer()) {
                    error(-1, "index operator [] can only be used on "
                              "array or pointer");
                    return exp_return_t(std::make_shared<VoidType>(), nullptr);
                }

                // calc the address through index. if the curr lval
                // is a pointer, don't forget to get the value from
                // pointer first
                if (lval_type->is_pointer()) {
                    lval_val = _builder.create_load(ir::Type::L, lval_val);
                }

                auto lval_indirect_type =
                    std::static_pointer_cast<IndirectType>(lval_type);

                auto elm_size = ir::ConstBits::get(
                    lval_indirect_type->get_base_type()->get_size());
                auto offset =
                    _builder.create_mul(ir::Type::W, exp_val, elm_size);
                auto val = _builder.create_add(ir::Type::L, lval_val, offset);

                return exp_return_t(lval_indirect_type->get_base_type(), val);
            }},
        node);
}

exp_return_t ASTVisitor::visitCallExp(const CallExp &node) {
    auto symbol = _current_scope->get_symbol(node.ident);
    if (!symbol) {
        error(-1, "undefined function " + node.ident);
        return exp_return_t(std::make_shared<VoidType>(), nullptr);
    }

    auto func_symbol = std::dynamic_pointer_cast<FunctionSymbol>(symbol);
    if (!func_symbol) {
        error(-1, node.ident + " is not a function");
        return exp_return_t(std::make_shared<VoidType>(), nullptr);
    }

    auto params_type = func_symbol->param_types;

    if (params_type.size() != node.func_rparams->size()) {
        error(-1, "params number not matched in function call " + node.ident);
        return exp_return_t(std::make_shared<VoidType>(), nullptr);
    }

    std::vector<ir::ValuePtr> ir_args;
    for (int i = 0; i < params_type.size(); i++) {
        auto [exp_type, exp_val] = visitExp(*node.func_rparams->at(i));
        // TODO: if params type is not matched, handle exception
        if (0 && exp_type != params_type[i]) {
            error(-1, "params type not matched in function call " + node.ident +
                          ", expected " + params_type[i]->tostring() +
                          ", got " + exp_type->tostring());
            return exp_return_t(std::make_shared<VoidType>(), nullptr);
        }
        ir_args.push_back(exp_val);
    }

    auto ir_func_ret = _builder.create_call(_type2irtype(func_symbol->type),
                                            func_symbol->value, ir_args);

    return exp_return_t(symbol->type, ir_func_ret);
}

exp_return_t ASTVisitor::visitUnaryExp(const UnaryExp &node) {
    auto [exp_type, exp_val] = visitExp(*node.exp);

    // TUDO: need to check op
    auto val = _builder.create_neg(_type2irtype(exp_type), exp_val);

    return exp_return_t(exp_type, val);
}

exp_return_t ASTVisitor::visitCompareExp(const CompareExp &node) {
    auto [left_type, left_val] = visitExp(*node.left);
    auto [right_type, right_val] = visitExp(*node.right);

    auto type = _calc_type(left_type, right_type);

    // TODO: type convert and support different ops
    auto val = _builder.create_ceqw(left_val, right_val);

    return exp_return_t(std::make_shared<Int32Type>(), val);
}

exp_return_t ASTVisitor::visitNumber(const Number &node) {
    return std::visit(
        overloaded{
            [this](int node) {
                return exp_return_t(std::make_shared<Int32Type>(), nullptr);
            },
            [this](float node) {
                return exp_return_t(std::make_shared<FloatType>(), nullptr);
            },
        },
        node);
}

cond_return_t ASTVisitor::visitCond(const Cond &node) {
    return std::visit(
        overloaded{
            [this](const Exp &node) {
                auto [type, val] = visitExp(node);

                // generate conditional jump instruction
                auto jnz_block = _builder.create_jnz(val, nullptr, nullptr);

                return cond_return_t(BlockPtrList{jnz_block},
                                     BlockPtrList{jnz_block});
            },
            [this](const LogicalExp &node) { return visitLogicalExp(node); },
        },
        node);
}

cond_return_t ASTVisitor::visitLogicalExp(const LogicalExp &node) {
    auto [ltruelist, lfalselist] = visitCond(*node.left);

    // TODO: create a new block for the right expression
    auto logic_right_block = _builder.create_label("logic_right");

    auto [rtruelist, rfalselist] = visitCond(*node.right);

    BlockPtrList truelist, falselist;
    if (node.op == LogicalExp::AND) {
        // if the logical expression is `&&`, we need to merge the false list
        // from left and right expression
        falselist.assign(lfalselist.begin(), lfalselist.end());
        falselist.insert(falselist.end(), rfalselist.begin(), rfalselist.end());

        // as for the true list from left expression, just jump to the right
        // expression

        // IR: jmp @logic_right (for each inst in ltruelist)

        // and the true list from right expression is the true list of the whole
        // expression
        truelist.assign(rtruelist.begin(), rtruelist.end());
    } else { // OR
        // vice versa, if the logical expression is `||`, we need to merge the
        // true list from left and right expression
        truelist.assign(ltruelist.begin(), ltruelist.end());
        truelist.insert(truelist.end(), rtruelist.begin(), rtruelist.end());

        // as for the false list from left expression, just jump to the right
        // expression

        // IR: jmp @logic_right (for each inst in lfalselist)

        // and the false list from right expression is the false list of the
        // whole expression
        falselist.assign(rfalselist.begin(), rfalselist.end());
    }

    _builder.create_label("logic_join");

    return cond_return_t(truelist, falselist);
}