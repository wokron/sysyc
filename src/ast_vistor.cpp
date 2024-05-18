#include "ast.h"
#include "ast_visitor.h"
#include "utils.h"
#include <variant>

std::shared_ptr<Type> asttype2type(ASTType type) {
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
        if (node.type == Decl::CONST) {
            visitConstDef(*elm, node.btype);
        } else {
            visitVarDef(*elm, node.btype);
        }
    }
}

void ASTVisitor::visitConstDef(const VarDef &node, ASTType btype) {
    auto type = visitDims(*node.dims, btype);

    std::shared_ptr<Initializer> initializer = nullptr;
    if (node.init_val != nullptr) {
        initializer = visitInitVal(*node.init_val, type);
    }

    auto symbol =
        std::make_shared<VariableSymbol>(node.ident, type, true, initializer);

    if (!currentScope->add_symbol(symbol)) {
        // TODO: handle exception
    }
}

void ASTVisitor::visitVarDef(const VarDef &node, ASTType btype) {
    auto type = visitDims(*node.dims, btype);

    std::shared_ptr<Initializer> initializer = nullptr;
    if (node.init_val != nullptr) {
        initializer = visitInitVal(*node.init_val, type);
    }

    auto symbol =
        std::make_shared<VariableSymbol>(node.ident, type, false, initializer);

    if (!currentScope->add_symbol(symbol)) {
        // TODO: handle exception
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
                    // TODO: handle exception
                }
                auto array_type = std::dynamic_pointer_cast<ArrayType>(type);

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
    auto base_type = asttype2type(btype);
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
    auto return_type = asttype2type(node.func_type);
    std::vector<std::shared_ptr<Type>> params_type;
    for (auto &param_symbol : param_symbols) {
        params_type.push_back(param_symbol->type);
    }

    // add function symbol
    auto symbol =
        std::make_shared<FunctionSymbol>(node.ident, params_type, return_type);
    if (!currentScope->add_symbol(symbol)) {
        // TODO: handle exception
    }

    // going into funciton scope
    currentScope = currentScope->push_scope();

    // add symbols of params to symbol table
    for (auto &param_symbol : param_symbols) {
        if (!currentScope->add_symbol(param_symbol)) {
            // TODO: handle exception
        }
    }

    visitBlockItems(*node.block);

    currentScope = currentScope->pop_scope();
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
    visitLVal(*node.lval);
    visitExp(*node.exp);
    // TODO: generate ir code
}

void ASTVisitor::visitExpStmt(const ExpStmt &node) {
    if (node.exp) {
        visitExp(*node.exp);
    }
    // TODO: generate ir code
}

void ASTVisitor::visitBlockStmt(const BlockStmt &node) {
    currentScope = currentScope->push_scope();
    visitBlockItems(*node.block);
    currentScope = currentScope->pop_scope();
}

void ASTVisitor::visitIfStmt(const IfStmt &node) {
    visitCond(*node.cond);

    currentScope = currentScope->push_scope();
    visitStmt(*node.if_stmt);
    currentScope = currentScope->pop_scope();

    if (node.else_stmt) {
        currentScope = currentScope->push_scope();
        visitStmt(*node.else_stmt);
        currentScope = currentScope->pop_scope();
    }
    // TODO: generate ir code
}

void ASTVisitor::visitWhileStmt(const WhileStmt &node) {
    visitCond(*node.cond);

    currentScope = currentScope->push_scope();
    visitStmt(*node.stmt);
    currentScope = currentScope->pop_scope();

    // TODO: generate ir code
}

void ASTVisitor::visitControlStmt(const ControlStmt &node) {
    if (node.type == ControlStmt::BREAK) {
        // TODO: generate ir code
    } else {
        // TODO: generate ir code
    }
}

void ASTVisitor::visitReturnStmt(const ReturnStmt &node) {
    if (node.exp) {
        visitExp(*node.exp);
    }
}

exp_return_t ASTVisitor::visitConstExp(const Exp &node) {
    // TODO: check if the expression is constant
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
    // TODO: return type and ir value
    return std::make_tuple(_calc_type(left_type, right_type), nullptr);
}

exp_return_t ASTVisitor::visitLValExp(const LValExp &node) {
    auto [type, val] = visitLVal(*node.lval);

    // TODO: since lval is in an expression, we need to get the value from the
    // lval address

    return std::make_tuple(type, val);
}

exp_return_t ASTVisitor::visitLVal(const LVal &node) {
    // TODO: return type and ir value
    return std::visit(
        overloaded{[this](const Ident &node) {
                       auto symbol = currentScope->get_symbol(node);
                       if (!symbol) {
                           // TODO: handle exception
                       }
                       return std::make_tuple(symbol->type, nullptr);
                   },
                   [this](const Index &node) {
                       auto [lval_type, lval_val] = visitLVal(*node.lval);
                       visitExp(*node.exp);

                       if (!lval_type->is_array() && !lval_type->is_pointer()) {
                           // TODO: handle exception
                       }

                       // TODO: calc the address through index. if the curr lval
                       // is a pointer, don't forget to get the value from
                       // pointer first

                       auto lval_indirect_type =
                           std::dynamic_pointer_cast<IndirectType>(lval_type);

                       return std::make_tuple(
                           lval_indirect_type->get_base_type(), nullptr);
                   }},
        node);
}

exp_return_t ASTVisitor::visitCallExp(const CallExp &node) {
    auto symbol = currentScope->get_symbol(node.ident);
    if (!symbol) {
        // TODO: handle exception
    }

    auto func_symbol = std::dynamic_pointer_cast<FunctionSymbol>(symbol);
    if (!func_symbol) {
        // TODO: handle exception
    }

    auto params_type = func_symbol->param_types;
    // TODO: if params type is not matched, handle exception

    for (auto &elm : *node.func_rparams) {
        visitExp(*elm);
    }
    // TODO: return type and ir value

    return std::make_tuple(symbol->type, nullptr);
}

exp_return_t ASTVisitor::visitUnaryExp(const UnaryExp &node) {
    auto [exp_type, exp_val] = visitExp(*node.exp);
    return std::make_tuple(exp_type, nullptr);
}

exp_return_t ASTVisitor::visitCompareExp(const CompareExp &node) {
    auto [left_type, left_val] = visitExp(*node.left);
    auto [right_type, right_val] = visitExp(*node.right);

    auto type = _calc_type(left_type, right_type);

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

void ASTVisitor::visitCond(const Cond &node) {
    std::visit(overloaded{
                   [this](const Exp &node) { visitExp(node); },
                   [this](const LogicalExp &node) { visitLogicalExp(node); },
               },
               node);
}

void ASTVisitor::visitLogicalExp(const LogicalExp &node) {
    visitCond(*node.left);
    visitCond(*node.right);
}