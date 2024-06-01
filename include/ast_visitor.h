#pragma once
#include "ast.h"
#include "ir/builder.h"
#include "ir/ir.h"
#include "midend/symbol.h"
#include <iostream>
#include <memory>
#include <stack>
#include <variant>

using ExpReturn = std::tuple<std::shared_ptr<Type>, ir::ValuePtr>;

using BlockPtrList = std::vector<ir::BlockPtr>;

// return type for conditional expression, first vector is the true path, second
// vector is the false path
using CondReturn = std::tuple<BlockPtrList, BlockPtrList>;

// jump insts for while loop, first vector for continue, second vector for break
using ContinueBreak = std::tuple<BlockPtrList, BlockPtrList>;

class ASTVisitor {
  private:
    std::shared_ptr<SymbolTable> _current_scope;
    ir::Module &_module;
    ir::IRBuilder _builder;

    std::shared_ptr<Type> _current_return_type = nullptr;
    std::stack<ContinueBreak> _while_stack;

  public:
    ASTVisitor(ir::Module &module)
        : _module(module),
          _current_scope(std::make_shared<SymbolTable>(nullptr)) {}

    // define methods for each AST node type
    void visit(const CompUnits &node);
    void visitDecl(const Decl &node);
    void visitVarDef(const VarDef &node, ASTType btype, bool is_const);
    std::shared_ptr<Initializer> visitInitVal(const InitVal &node, Type &type);
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

    ExpReturn visitConstExp(const Exp &node);
    ExpReturn visitExp(const Exp &node);
    ExpReturn visitBinaryExp(const BinaryExp &node);
    ExpReturn visitLValExp(const LValExp &node);
    ExpReturn visitCallExp(const CallExp &node);
    ExpReturn visitUnaryExp(const UnaryExp &node);
    ExpReturn visitCompareExp(const CompareExp &node);
    ExpReturn visitNumber(const Number &node);
    ExpReturn visitLVal(const LVal &node);

    CondReturn visitCond(const Cond &node);
    CondReturn visitLogicalExp(const LogicalExp &node);

  private:
    // some utility methods
    bool _is_global_context() const { return !_current_scope->has_parent(); }

    ir::ValuePtr _convert_if_needed(const Type &to, const Type &from,
                                    ir::ValuePtr val);

    static std::shared_ptr<Type> _asttype2type(ASTType type);

    static ir::Type _type2irtype(const Type &type);

    static ir::ConstBitsPtr _convert_const(ir::Type target_type,
                                           const ir::ConstBits &const_val);

    static void _init_global(ir::Data &data, const Type &elm_type,
                             const Initializer &initializer);
};