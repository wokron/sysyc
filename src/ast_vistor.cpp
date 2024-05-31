#include "ast.h"
#include "ast_visitor.h"
#include "error.h"
#include "utils.h"
#include <variant>

static std::shared_ptr<Type> _asttype2type(ASTType type) {
    switch (type) {
    case ASTType::INT:
        return Int32Type::get();
    case ASTType::FLOAT:
        return FloatType::get();
    case ASTType::VOID:
        return VoidType::get();
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
            auto [exp_type, exp_val] = visitConstExp(*exp);
            if (exp_type->is_int32() || exp_type->is_float()) {
                auto exp_const_val =
                    std::dynamic_pointer_cast<ir::ConstBits>(exp_val);
                if (auto val = std::get_if<int>(&(exp_const_val->value))) {
                    tb.in_array(*val);
                } else if (auto val =
                               std::get_if<float>(&(exp_const_val->value))) {
                    tb.in_array(*val); // implicit cast to int
                } else {
                    throw std::logic_error("no value in const bits");
                }
            } else {
                error(-1, "array size must an int value");
                tb.in_array(1); // just to make it work
            }
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
    _current_return_type = symbol->type;

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

    _builder.set_function(nullptr);

    _current_return_type = nullptr;
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

std::shared_ptr<ir::Value>
ASTVisitor::convert_if_needed(std::shared_ptr<Type> to,
                              std::shared_ptr<Type> from,
                              std::shared_ptr<ir::Value> val) {
    if (to == from) {
        return val;
    } else if (to->is_int32() && from->is_float()) {
        return _builder.create_stosi(val);
    } else if (to->is_float() && from->is_int32()) {
        return _builder.create_swtof(val);
    } else {
        error(-1, "type convert not supported");
        return nullptr;
    }
}

void ASTVisitor::visitAssignStmt(const AssignStmt &node) {
    auto [exp_type, exp_val] = visitExp(*node.exp);
    auto [lval_type, lval_val] = visitLVal(*node.lval);

    if (lval_type->is_error() || exp_type->is_error()) {
        return;
    }

    if (lval_type->is_array() || lval_type->is_pointer()) {
        error(-1, "left side of assignment is not ");
        return;
    }

    exp_val = convert_if_needed(lval_type, exp_type, exp_val);
    if (!exp_val) {
        error(-1, "type not matched in assignment");
        return;
    }

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
    auto [jmp_to_true, jmp_to_false] = visitCond(*node.cond);

    auto true_block = _builder.create_label("if_true");

    _current_scope = _current_scope->push_scope();
    visitStmt(*node.if_stmt);
    _current_scope = _current_scope->pop_scope();

    std::shared_ptr<ir::Block> jmp_to_join;
    if (node.else_stmt) {
        jmp_to_join = _builder.create_jmp(nullptr);
    }

    auto false_block = _builder.create_label("if_false");

    if (node.else_stmt) {
        _current_scope = _current_scope->push_scope();
        visitStmt(*node.else_stmt);
        _current_scope = _current_scope->pop_scope();

        auto join_block = _builder.create_label("if_join");
        jmp_to_join->jump.blk[0] = join_block;
    }

    // fill jump targets
    for (auto &jmp_block : jmp_to_true) {
        jmp_block->jump.blk[0] = true_block;
    }
    for (auto &jmp_block : jmp_to_false) {
        jmp_block->jump.blk[1] = false_block;
    }
}

void ASTVisitor::visitWhileStmt(const WhileStmt &node) {
    auto cond_block = _builder.create_label("while_cond");

    auto [jmp_to_true, jmp_to_false] = visitCond(*node.cond);

    auto body_block = _builder.create_label("while_body");

    _current_scope = _current_scope->push_scope();
    visitStmt(*node.stmt);
    _current_scope = _current_scope->pop_scope();

    _builder.create_jmp(cond_block);

    auto join_block = _builder.create_label("while_join");

    // fill jump targets
    for (auto &jmp_block : jmp_to_true) {
        jmp_block->jump.blk[0] = body_block;
    }
    for (auto &jmp_block : jmp_to_false) {
        jmp_block->jump.blk[1] = join_block;
    }
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
        if (exp_type->is_error()) {
            return;
        }

        exp_val = convert_if_needed(_current_return_type, exp_type, exp_val);
        if (!exp_val) {
            error(-1, "type not matched in return statement");
            return;
        }

        _builder.create_ret(exp_val);
    }
    _builder.create_ret(nullptr);
}

exp_return_t ASTVisitor::visitConstExp(const Exp &node) {
    auto [type, val] = visitExp(node);

    if (type->is_error()) {
        return exp_return_t(ErrorType::get(), nullptr);
    }

    // check if the expression is constant
    if (auto const_val = std::dynamic_pointer_cast<ir::Const>(val);
        !const_val) {
        error(-1, "not a const expression");
        return exp_return_t(ErrorType::get(), nullptr);
    }

    return exp_return_t(type, val);
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

exp_return_t ASTVisitor::visitBinaryExp(const BinaryExp &node) {
    auto [left_type, left_val] = visitExp(*node.left);
    auto [right_type, right_val] = visitExp(*node.right);

    if (left_type->is_error() || right_type->is_error()) {
        return exp_return_t(ErrorType::get(), nullptr);
    }

    auto type = left_type->implicit_cast(*right_type);
    left_val = convert_if_needed(type, left_type, left_val);
    if (!left_val) {
        error(-1, "type not matched in binary expression");
        return exp_return_t(ErrorType::get(), nullptr);
    }
    right_val = convert_if_needed(type, right_type, right_val);
    if (!right_val) {
        error(-1, "type not matched in binary expression");
        return exp_return_t(ErrorType::get(), nullptr);
    }

    auto ir_type = _type2irtype(type);
    switch (node.op) {
    case BinaryExp::ADD:
        return exp_return_t(type,
                            _builder.create_add(ir_type, left_val, right_val));
    case BinaryExp::SUB:
        return exp_return_t(type,
                            _builder.create_sub(ir_type, left_val, right_val));
    case BinaryExp::MULT:
        return exp_return_t(type,
                            _builder.create_mul(ir_type, left_val, right_val));
    case BinaryExp::DIV:
        return exp_return_t(type,
                            _builder.create_div(ir_type, left_val, right_val));
    case BinaryExp::MOD:
        return exp_return_t(type,
                            _builder.create_rem(ir_type, left_val, right_val));
    default:
        throw std::logic_error("unreachable");
    }
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
                    return exp_return_t(VoidType::get(), nullptr);
                }

                return exp_return_t(symbol->type, symbol->value);
            },
            [this](const Index &node) {
                auto [lval_type, lval_val] = visitLVal(*node.lval);
                auto [exp_type, exp_val] = visitExp(*node.exp);

                if (!lval_type->is_array() && !lval_type->is_pointer()) {
                    error(-1, "index operator [] can only be used on "
                              "array or pointer");
                    return exp_return_t(VoidType::get(), nullptr);
                }

                // calc the address through index. if the curr lval
                // is a pointer, don't forget to get the value from
                // pointer first
                if (lval_type->is_pointer()) {
                    lval_val = _builder.create_load(ir::Type::L, lval_val);
                }

                auto lval_indirect_type =
                    std::static_pointer_cast<IndirectType>(lval_type);

                // since exp_type is int, we need to convert it to long
                exp_val = _builder.create_extsw(exp_val);

                auto elm_size = ir::ConstBits::get(
                    lval_indirect_type->get_base_type()->get_size());

                auto offset =
                    _builder.create_mul(ir::Type::L, exp_val, elm_size);
                auto val = _builder.create_add(ir::Type::L, lval_val, offset);

                // addr = base_addr + index * elm_size
                return exp_return_t(lval_indirect_type->get_base_type(), val);
            }},
        node);
}

exp_return_t ASTVisitor::visitCallExp(const CallExp &node) {
    auto symbol = _current_scope->get_symbol(node.ident);
    if (!symbol) {
        error(-1, "undefined function " + node.ident);
        return exp_return_t(ErrorType::get(), nullptr);
    }

    auto func_symbol = std::dynamic_pointer_cast<FunctionSymbol>(symbol);
    if (!func_symbol) {
        error(-1, node.ident + " is not a function");
        return exp_return_t(ErrorType::get(), nullptr);
    }

    auto params_type = func_symbol->param_types;

    if (params_type.size() != node.func_rparams->size()) {
        error(-1, "params number not matched in function call " + node.ident);
        return exp_return_t(ErrorType::get(), nullptr);
    }

    std::vector<ir::ValuePtr> ir_args;
    for (int i = 0; i < params_type.size(); i++) {
        auto [exp_type, exp_val] = visitExp(*node.func_rparams->at(i));

        if (exp_type->is_error()) {
            return exp_return_t(ErrorType::get(), nullptr);
        }

        if (exp_type != params_type[i]) {
            error(-1, "params type not matched in function call " + node.ident +
                          ", expected " + params_type[i]->tostring() +
                          ", got " + exp_type->tostring());
            return exp_return_t(ErrorType::get(), nullptr);
        }

        ir_args.push_back(exp_val);
    }

    auto ir_ret = _builder.create_call(_type2irtype(func_symbol->type),
                                       func_symbol->value, ir_args);

    return exp_return_t(symbol->type, ir_ret);
}

exp_return_t ASTVisitor::visitUnaryExp(const UnaryExp &node) {
    auto [exp_type, exp_val] = visitExp(*node.exp);

    if (exp_type->is_error()) {
        return exp_return_t(ErrorType::get(), nullptr);
    }

    switch (node.op) {
    case UnaryExp::ADD:
        return exp_return_t(exp_type, exp_val); // do nothing
    case UnaryExp::SUB:
        if (!exp_type->is_int32() && !exp_type->is_float()) {
            error(-1, "neg operator can only be used on int or float");
            return exp_return_t(ErrorType::get(), nullptr);
        }
        return exp_return_t(
            exp_type, _builder.create_neg(_type2irtype(exp_type), exp_val));
    case UnaryExp::NOT:
        if (!exp_type->is_int32()) {
            error(-1, "not operator can only be used on int");
            return exp_return_t(ErrorType::get(), nullptr);
        }
        // !a equal to (a == 0)
        return exp_return_t(
            exp_type, _builder.create_ceqw(exp_val, ir::ConstBits::get(0)));
    default:
        throw std::logic_error("unreachable");
    }
}

exp_return_t ASTVisitor::visitCompareExp(const CompareExp &node) {
    auto [left_type, left_val] = visitExp(*node.left);
    auto [right_type, right_val] = visitExp(*node.right);

    if (left_type->is_error() || right_type->is_error()) {
        return exp_return_t(ErrorType::get(), nullptr);
    }

    auto type = left_type->implicit_cast(*right_type);
    left_val = convert_if_needed(type, left_type, left_val);
    if (!left_val) {
        error(-1, "type not matched in compare expression");
        return exp_return_t(ErrorType::get(), nullptr);
    }
    right_val = convert_if_needed(type, right_type, right_val);
    if (!right_val) {
        error(-1, "type not matched in compare expression");
        return exp_return_t(ErrorType::get(), nullptr);
    }

    if (!type->is_int32() && !type->is_float()) {
        error(-1, "compare operator can only be used on int or float");
        return exp_return_t(ErrorType::get(), nullptr);
    }

    if (type->is_int32()) {
        switch (node.op) {
        case CompareExp::EQ:
            return exp_return_t(Int32Type::get(),
                                _builder.create_ceqw(left_val, right_val));
        case CompareExp::NE:
            return exp_return_t(Int32Type::get(),
                                _builder.create_cnew(left_val, right_val));
        case CompareExp::LT:
            return exp_return_t(Int32Type::get(),
                                _builder.create_cslew(left_val, right_val));
        case CompareExp::LE:
            return exp_return_t(Int32Type::get(),
                                _builder.create_cslew(right_val, left_val));
        case CompareExp::GT:
            return exp_return_t(Int32Type::get(),
                                _builder.create_cslew(right_val, left_val));
        case CompareExp::GE:
            return exp_return_t(Int32Type::get(),
                                _builder.create_cslew(left_val, right_val));
        default:
            throw std::logic_error("unreachable");
        }
    } else { // float
        switch (node.op) {
        case CompareExp::EQ:
            return exp_return_t(Int32Type::get(),
                                _builder.create_ceqs(left_val, right_val));
        case CompareExp::NE:
            return exp_return_t(Int32Type::get(),
                                _builder.create_cnes(left_val, right_val));
        case CompareExp::LT:
            return exp_return_t(Int32Type::get(),
                                _builder.create_csltw(left_val, right_val));
        case CompareExp::LE:
            return exp_return_t(Int32Type::get(),
                                _builder.create_csgew(left_val, right_val));
        case CompareExp::GT:
            return exp_return_t(Int32Type::get(),
                                _builder.create_csgtw(left_val, right_val));
        case CompareExp::GE:
            return exp_return_t(Int32Type::get(),
                                _builder.create_csltw(right_val, left_val));
        default:
            throw std::logic_error("unreachable");
        }
    }
}

exp_return_t ASTVisitor::visitNumber(const Number &node) {
    return std::visit(
        overloaded{
            [this](int node) {
                return exp_return_t(Int32Type::get(), ir::ConstBits::get(node));
            },
            [this](float node) {
                return exp_return_t(FloatType::get(), ir::ConstBits::get(node));
            },
        },
        node);
}

cond_return_t ASTVisitor::visitCond(const Cond &node) {
    return std::visit(
        overloaded{
            [this](const Exp &node) {
                auto [type, val] = visitExp(node);
                if (type->is_error() || !type->is_int32()) {
                    return cond_return_t(BlockPtrList{}, BlockPtrList{});
                }

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

    // create a new block for the right expression
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
        for (auto &jmp_block : ltruelist) {
            jmp_block->jump.blk[0] = logic_right_block;
        }

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
        for (auto &jmp_block : lfalselist) {
            jmp_block->jump.blk[1] = logic_right_block;
        }

        // and the false list from right expression is the false list of the
        // whole expression
        falselist.assign(rfalselist.begin(), rfalselist.end());
    }

    _builder.create_label("logic_join");

    return cond_return_t(truelist, falselist);
}