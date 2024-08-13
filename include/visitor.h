#pragma once
#include "ast.h"
#include "ir/builder.h"
#include "ir/ir.h"
#include "sym/symbol.h"
#include <iostream>
#include <memory>
#include <stack>
#include <variant>

using ExpReturn = std::tuple<sym::TypePtr, ir::ValuePtr>;

using BlockPtrList = std::vector<ir::BlockPtr>;

// return type for conditional expression, first vector is the true path, second
// vector is the false path
using CondReturn = std::tuple<BlockPtrList, BlockPtrList>;

// jump insts for while loop, first vector for continue, second vector for break
using ContinueBreak = std::tuple<BlockPtrList, BlockPtrList>;

using ConstLValReturn =
    std::tuple<sym::TypePtr, std::map<int, sym::Initializer::InitValue>>;

class Visitor {
private:
    sym::SymbolTablePtr _current_scope;
    ir::Module &_module;
    ir::IRBuilder _builder;

    sym::TypePtr _current_return_type = nullptr;
    std::stack<ContinueBreak> _while_stack;

    // since visit_const_exp could be called multiple times, we need to
    // record the count to make the func re-entrant
    int _require_const_lval = 0;

    bool _optimize = false;

    std::unordered_map<ir::ValuePtr, ir::ValuePtr> _last_store;
    bool _in_unroll_loop = false;

public:
    Visitor(ir::Module &module, bool optimize = false)
        : _module(module),
          _current_scope(std::make_shared<sym::SymbolTable>(nullptr)),
          _optimize(optimize) {
        _add_builtin_funcs();
    }

    // define methods for each AST node type

    /**
     * @brief Visit the whole AST
     * @param node The root node of the AST
     */
    void visit(const CompUnits &node);

    void visit_decl(const Decl &node);
    void visit_var_def(const VarDef &node, ASTType btype, bool is_const);
    sym::InitializerPtr visit_init_val(const InitVal &node, sym::Type &type,
                                       bool is_const);
    sym::TypePtr visit_dims(const Dims &node, ASTType btype);

    void visit_func_def(const FuncDef &node);
    std::vector<sym::SymbolPtr> visit_func_fparams(const FuncFParams &node);

    void visit_block_items(const BlockItems &node);

    void visit_stmt(const Stmt &node);
    void visit_assign_stmt(const AssignStmt &node);
    void visit_exp_stmt(const ExpStmt &node);
    void visit_block_stmt(const BlockStmt &node);
    void visit_if_stmt(const IfStmt &node);
    void visit_while_stmt(const WhileStmt &node);
    void visit_control_stmt(const ControlStmt &node);
    void visit_return_stmt(const ReturnStmt &node);

    ExpReturn visit_const_exp(const Exp &node);
    ExpReturn visit_exp(const Exp &node);
    ExpReturn visit_binary_exp(const BinaryExp &node);
    ExpReturn visit_lval_exp(const LValExp &node);
    ExpReturn visit_call_exp(const CallExp &node);
    ExpReturn visit_unary_exp(const UnaryExp &node);
    ExpReturn visit_compare_exp(const CompareExp &node);
    ExpReturn visit_number(const Number &node);
    ExpReturn visit_lval(const LVal &node);
    ConstLValReturn visit_const_lval(const LVal &node);

    CondReturn visit_cond(const Cond &node);
    CondReturn visit_logical_exp(const LogicalExp &node);

private:
    // some utility methods

    /**
     * @brief Add built-in functions to the global scope
     */
    void _add_builtin_funcs();

    /**
     * @brief Check if the current context is global
     * @return true if the current context is global, false otherwise
     */
    bool _is_global_context() const { return !_current_scope->has_parent(); }

    /**
     * @brief Convert a value to a specific type if needed
     * @param to The target type
     * @param from The original type
     * @param val The value to be converted
     * @return The converted value
     */
    ir::ValuePtr _convert_if_needed(const sym::Type &to, const sym::Type &from,
                                    ir::ValuePtr val);

    /**
     * @brief Convert an AST type to a symbol type
     * @param type The AST type
     * @return The symbol type
     */
    static sym::TypePtr _asttype2symtype(ASTType type);

    /**
     * @brief Convert a symbol type to an IR type
     * @param type The symbol type
     * @return The IR type
     */
    static ir::Type _symtype2irtype(const sym::Type &type);

    /**
     * @brief Convert a constant value to a specific type
     * @param target_type The target type
     * @param const_val The constant value
     * @return The converted constant value
     */
    static ir::ConstBitsPtr _convert_const(ir::Type target_type,
                                           const ir::ConstBits &const_val);

    /**
     * @brief Initialize a global variable by building ir::Data
     * @param data The ir::Data to be initialized
     * @param elm_type The element type of the data
     * @param initializer The initializer of the data
     */
    static void _init_global(ir::Data &data, const sym::Type &elm_type,
                             const sym::Initializer &initializer);


    bool _can_unroll_loop(const WhileStmt &node, int &from, int &to, bool &is_mini_loop);

    bool _has_control_stmt(const Stmt &node);
};