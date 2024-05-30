#pragma once
#include "ast.h"
#include "ir/ir.h"
#include "midend/symbol.h"
#include <iostream>
#include <memory>
#include <variant>

using exp_return_t =
    std::tuple<std::shared_ptr<Type>, std::shared_ptr<ir::Value>>;

// return type for conditional expression, first vector is the true path, second
// vector is the false path
using BlockPtrList = std::vector<std::shared_ptr<ir::Block>>;
using cond_return_t = std::tuple<BlockPtrList, BlockPtrList>;

class ASTVisitor {
  private:
    std::shared_ptr<SymbolTable> _current_scope;

  public:
    ASTVisitor() : _current_scope(std::make_shared<SymbolTable>(nullptr)) {}

    ASTVisitor(std::shared_ptr<SymbolTable> scope) : _current_scope(scope) {}

    // define methods for each AST node type
    void visit(const CompUnits &node);
    void visitDecl(const Decl &node);
    void visitVarDef(const VarDef &node, ASTType btype, bool is_const);
    std::shared_ptr<Initializer> visitInitVal(const InitVal &node,
                                              std::shared_ptr<Type> type);
    std::shared_ptr<Type> visitDims(const Dims &node, ASTType btype);
    void visitFuncDef(const FuncDef &node);
    std::vector<std::shared_ptr<Symbol>>
    visitFuncFParams(const FuncFParams &node);
    void visitBlockItems(const BlockItems &node);

    void visitStmt(const Stmt &node);
    void visitAssignStmt(const AssignStmt &node);
    void visitExpStmt(const ExpStmt &node);
    void visitBlockStmt(const BlockStmt &node);
    void visitIfStmt(const IfStmt &node);
    void visitWhileStmt(const WhileStmt &node);
    void visitControlStmt(const ControlStmt &node);
    void visitReturnStmt(const ReturnStmt &node);

    exp_return_t visitConstExp(const Exp &node);
    exp_return_t visitExp(const Exp &node);
    exp_return_t visitBinaryExp(const BinaryExp &node);
    exp_return_t visitLValExp(const LValExp &node);
    exp_return_t visitCallExp(const CallExp &node);
    exp_return_t visitUnaryExp(const UnaryExp &node);
    exp_return_t visitCompareExp(const CompareExp &node);
    exp_return_t visitNumber(const Number &node);
    exp_return_t visitLVal(const LVal &node);

    cond_return_t visitCond(const Cond &node);
    cond_return_t visitLogicalExp(const LogicalExp &node);

    // some utility methods
    bool is_global_context() const { return !_current_scope->has_parent(); }
};