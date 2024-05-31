#pragma once
#include "ast.h"
#include "ir/builder.h"
#include "ir/ir.h"
#include "midend/symbol.h"
#include <iostream>
#include <memory>
#include <stack>
#include <variant>

using exp_return_t =
    std::tuple<std::shared_ptr<Type>, std::shared_ptr<ir::Value>>;

// return type for conditional expression, first vector is the true path, second
// vector is the false path
using BlockPtrList = std::vector<std::shared_ptr<ir::Block>>;
using cond_return_t = std::tuple<BlockPtrList, BlockPtrList>;

// jump insts for while loop, first vector for continue, second vector for break
using while_jump_t = std::tuple<BlockPtrList, BlockPtrList>;

class ASTVisitor {
  private:
    std::shared_ptr<SymbolTable> _current_scope;
    ir::Module &_module;
    ir::IRBuilder _builder;

    std::shared_ptr<Type> _current_return_type = nullptr;
    std::stack<while_jump_t> _while_stack;

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

  private:
    // some utility methods
    bool _is_global_context() const { return !_current_scope->has_parent(); }

    std::shared_ptr<ir::Value>
    _convert_if_needed(std::shared_ptr<Type> to, std::shared_ptr<Type> from,
                       std::shared_ptr<ir::Value> val);

    static std::shared_ptr<Type> _asttype2type(ASTType type);

    static ir::Type _type2irtype(Type &type);

    static std::shared_ptr<ir::ConstBits>
    _convert_const(ir::Type target_type, ir::ConstBits &const_val);

    static void _init_global(ir::Data &data, Type &elm_type,
                             const Initializer &initializer);
};