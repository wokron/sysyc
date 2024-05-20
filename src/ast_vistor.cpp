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
        // IR: export data = { <init> }
    } else {
        // IR: %<reg> =<type> alloc (in @start)
        // IR: store<type> %<val> %<addr>
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
    for (auto &param_symbol : param_symbols) {
        params_type.push_back(param_symbol->type);
    }

    // add function symbol
    auto symbol =
        std::make_shared<FunctionSymbol>(node.ident, params_type, return_type);
    if (!_current_scope->add_symbol(symbol)) {
        error(-1, "redefine function " + node.ident);
        return;
    }

    // IR: export function <type> $<name>(<params>)

    // going into funciton scope
    _current_scope = _current_scope->push_scope();

    // IR: @start

    // add symbols of params to symbol table
    for (auto &param_symbol : param_symbols) {
        if (!_current_scope->add_symbol(param_symbol)) {
            error(-1, "redefine parameter " + param_symbol->name);
        }
        // IR: %<reg> =<type> alloc
        // IR: store<type> %<val> %<addr>
    }

    // IR: @body

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
    // IR: store<type> %<exp_val> %<lval_val>
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
    visitCond(*node.cond);

    // IR: jmp @if_true

    _current_scope = _current_scope->push_scope();
    visitStmt(*node.if_stmt);
    _current_scope = _current_scope->pop_scope();

    if (node.else_stmt) {
        // IR: jmp @if_join
    }
    // IR: @if_false

    if (node.else_stmt) {
        _current_scope = _current_scope->push_scope();
        visitStmt(*node.else_stmt);
        _current_scope = _current_scope->pop_scope();
        // IR: @if_join
    }
}

void ASTVisitor::visitWhileStmt(const WhileStmt &node) {
    // IR: @while_cond
    visitCond(*node.cond);

    // IR: @while_body

    _current_scope = _current_scope->push_scope();
    visitStmt(*node.stmt);
    _current_scope = _current_scope->pop_scope();

    // IR: jmp @while_cond
    // IR: @while_join
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
        // IR: ret %<exp_val>
    }
    // IR: ret
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
            return std::make_shared<VoidType>(); // error
        }
    } else {
        return std::make_shared<VoidType>(); // error
    }
}

exp_return_t ASTVisitor::visitBinaryExp(const BinaryExp &node) {
    auto [left_type, left_val] = visitExp(*node.left);
    auto [right_type, right_val] = visitExp(*node.right);

    // IR: %<reg> =<type> <op> %<left_val> %<right_val>

    return std::make_tuple(_calc_type(left_type, right_type), nullptr);
}

exp_return_t ASTVisitor::visitLValExp(const LValExp &node) {
    auto [type, val] = visitLVal(*node.lval);

    // since lval is in an expression, we need to get the value from the
    // lval address

    // IR: %<reg> =<type> load<type> %<val>

    return std::make_tuple(type, val);
}

exp_return_t ASTVisitor::visitLVal(const LVal &node) {
    return std::visit(
        overloaded{[this](const Ident &node) {
                       auto symbol = _current_scope->get_symbol(node);
                       if (!symbol) {
                           error(-1, "undefined symbol " + node);
                           return std::make_tuple<std::shared_ptr<Type>, Value>(
                               std::make_shared<VoidType>(), nullptr);
                       }

                       return std::make_tuple(symbol->type, nullptr);
                   },
                   [this](const Index &node) {
                       auto [lval_type, lval_val] = visitLVal(*node.lval);
                       auto [exp_type, exp_val] = visitExp(*node.exp);

                       if (!lval_type->is_array() && !lval_type->is_pointer()) {
                           error(-1, "index operator [] can only be used on "
                                     "array or pointer");
                           return std::make_tuple<std::shared_ptr<Type>, Value>(
                               std::make_shared<VoidType>(), nullptr);
                       }

                       // calc the address through index. if the curr lval
                       // is a pointer, don't forget to get the value from
                       // pointer first

                       if (lval_type->is_pointer()) {
                           // IR: %<reg> =<type> load<type> %<lval_val>
                       }
                       // IR: %<offset> =<type> mul %<exp_val> %<elm_size>
                       // IR: %<reg> =<type> add %<lval_val> %<offset>

                       auto lval_indirect_type =
                           std::static_pointer_cast<IndirectType>(lval_type);

                       return std::make_tuple<std::shared_ptr<Type>, Value>(
                           lval_indirect_type->get_base_type(), nullptr);
                   }},
        node);
}

exp_return_t ASTVisitor::visitCallExp(const CallExp &node) {
    auto symbol = _current_scope->get_symbol(node.ident);
    if (!symbol) {
        error(-1, "undefined function " + node.ident);
        return std::make_tuple(std::make_shared<VoidType>(), nullptr);
    }

    auto func_symbol = std::dynamic_pointer_cast<FunctionSymbol>(symbol);
    if (!func_symbol) {
        error(-1, node.ident + " is not a function");
        return std::make_tuple(std::make_shared<VoidType>(), nullptr);
    }

    auto params_type = func_symbol->param_types;

    if (params_type.size() != node.func_rparams->size()) {
        error(-1, "params number not matched in function call " + node.ident);
        return std::make_tuple(std::make_shared<VoidType>(), nullptr);
    }

    for (int i = 0; i < params_type.size(); i++) {
        auto [exp_type, exp_val] = visitExp(*node.func_rparams->at(i));
        // TODO: if params type is not matched, handle exception
        if (0 && exp_type != params_type[i]) {
            error(-1, "params type not matched in function call " + node.ident +
                          ", expected " + params_type[i]->tostring() +
                          ", got " + exp_type->tostring());
            return std::make_tuple(std::make_shared<VoidType>(), nullptr);
        }
    }

    // IR: %<reg> =<type> call $<name>(<params>)

    return std::make_tuple(symbol->type, nullptr);
}

exp_return_t ASTVisitor::visitUnaryExp(const UnaryExp &node) {
    auto [exp_type, exp_val] = visitExp(*node.exp);

    // IR: %<reg> =<type> <op> %<exp_val>, 0

    return std::make_tuple(exp_type, nullptr);
}

exp_return_t ASTVisitor::visitCompareExp(const CompareExp &node) {
    auto [left_type, left_val] = visitExp(*node.left);
    auto [right_type, right_val] = visitExp(*node.right);

    auto type = _calc_type(left_type, right_type);

    // IR: %<reg> =<type> <op> %<left_val> %<right_val>

    return std::make_tuple(std::make_shared<Int32Type>(), nullptr);
}

exp_return_t ASTVisitor::visitNumber(const Number &node) {
    return std::visit(overloaded{
                          [this](int node) {
                              return std::make_tuple<std::shared_ptr<Type>,
                                                     std::shared_ptr<Value>>(
                                  std::make_shared<Int32Type>(), nullptr);
                          },
                          [this](float node) {
                              return std::make_tuple<std::shared_ptr<Type>,
                                                     std::shared_ptr<Value>>(
                                  std::make_shared<FloatType>(), nullptr);
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
                // IR: jnz %<val> @true @false

                JumpInst inst = nullptr;
                return std::make_tuple(std::vector<JumpInst>{inst},
                                       std::vector<JumpInst>{inst});
            },
            [this](const LogicalExp &node) { return visitLogicalExp(node); },
        },
        node);
}

cond_return_t ASTVisitor::visitLogicalExp(const LogicalExp &node) {
    auto [ltruelist, lfalselist] = visitCond(*node.left);

    // TODO: create a new block for the right expression
    // IR: @logic_right

    auto [rtruelist, rfalselist] = visitCond(*node.right);

    std::vector<JumpInst> truelist, falselist;
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

    // IR: @logic_join

    return std::make_tuple(truelist, falselist);
}