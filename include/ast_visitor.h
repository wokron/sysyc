#pragma once
#include "ast.h"
#include "midend/symbol.h"
#include <iostream>
#include <memory>
#include <variant>

using exp_return_t = std::tuple<std::shared_ptr<Type>, std::shared_ptr<Value>>;

class ASTVisitor {
  public:
    std::shared_ptr<SymbolTable> currentScope;
    int loop_count = 0;

    ASTVisitor(std::shared_ptr<SymbolTable> startScope = nullptr)
        : currentScope(startScope ? startScope
                                  : std::make_shared<SymbolTable>(nullptr)) {}

    // Define methods for each AST node type
    void visit(const CompUnits &node);
    void visitDecl(const Decl &node);
    void visitConstDef(const VarDef &node, ASTType btype);
    void visitVarDef(const VarDef &node, ASTType btype);
    std::shared_ptr<Initializer> visitInitVal(const InitVal &node, std::shared_ptr<Type> type);
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
    exp_return_t visitLVal(const LVal &node);
    exp_return_t visitCallExp(const CallExp &node);
    exp_return_t visitUnaryExp(const UnaryExp &node);
    exp_return_t visitCompareExp(const CompareExp &node);
    exp_return_t visitNumber(const Number &node);

    void visitCond(const Cond &node);
    void visitLogicalExp(const LogicalExp &node);

    // Utility methods to determine the context of the current block of code
    bool isGlobalContext() const { return !currentScope->has_parent(); }

    // Other utility methods can be defined here as needed
};