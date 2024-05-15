#pragma once
#include "midend/symbol.h"
#include "ast.h"
#include <memory>
#include <variant>
#include <iostream>

class ASTVisitor {
public:
    std::shared_ptr<SymbolTable> current_scope;
    int loop_count = 0;

    ASTVisitor(std::shared_ptr<SymbolTable> startScope = nullptr)
        : currentScope(startScope ? startScope : std::make_shared<SymbolTable>(nullptr)) {}

    void visit(const std::shared_ptr<Stmt>& stmt) {
        std::visit([this](auto&& arg) { visit(arg); }, *stmt);
    }

    // Define methods for each AST node type
    void visit(const BinaryExp& exp);
    void visit(const BoolExp& exp);
    void visit(const CallExp& exp);
    void visit(const UnaryExp& exp);
    void visit(const CompareExp& exp);
    void visit(const AssignStmt& stmt);
    void visit(const ExpStmt& stmt);
    void visit(const BlockStmt& stmt);
    void visit(const IfStmt& stmt);
    void visit(const WhileStmt& stmt);
    void visit(const ControlStmt& stmt);
    void visit(const ReturnStmt& stmt);
    void visit(const VarDef& var_def);
    void visit(const Decl& decl);
    void visit(const FuncFParam& param);
    void visit(const FuncDef& func_def);

    void visitBlock(const BlockStmt& block, bool newScope);


    // Utility methods to determine the context of the current block of code
    bool isGlobalContext() const {
        return !currentScope->hasParent();
    }

    // Other utility methods can be defined here as needed
};