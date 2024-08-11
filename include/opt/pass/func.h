#pragma once

#include "opt/pass/base.h"

namespace opt {

/**
 * @brief A pass that fills the `is_leaf` field of each function.
 * A function is a leaf function if it does not call any other functions.
 * @note Nothing is required before this pass.
 */
class FillLeafPass : public ModulePass {
public:
    bool run_on_module(ir::Module &module) override;

private:
    bool _is_leaf_function(ir::Function &func);
};

/**
 * @brief A pass that fills the `is_inline` field of each function.
 * @note Nothing is required before this pass.
 */
class FillInlinePass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;

private:
    bool _is_inline(const ir::Function &func);
};

/**
 * @brief A pass that performs function inlining.
 * @note Requires `FillInlinePass`.
 * @note This pass will break every CFG-related pass.
 */
class FunctionInliningPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;

private:
    void _do_inline(ir::BlockPtr prev, ir::Function &inline_func,
                    ir::Function &target_func,
                    const std::vector<ir::ValuePtr> &args,
                    ir::TempPtr ret_target);
};

class TailRecursionElimination : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;

private:
    void _create_jump_target_block(ir::Function &func);
    bool _is_tail_recursive(const ir::Function &func, const ir::Block &block);
};

} // namespace opt
