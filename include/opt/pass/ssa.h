#pragma once

#include "opt/pass/base.h"
#include <stack>

namespace opt {

/**
 * @brief A pass that converts memory allocation instructions to register as
 * much as possible.
 * @note This pass requires `FillUsesPass` to be run before.
 * @warning This pass will only break use relationship in use-def, so a temp can
 * find its def, but cannot find its use.
 */
class MemoryToRegisterPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;

private:
    bool _mem_to_reg(ir::InstPtr alloc_inst);
    bool _is_able_to_reg(ir::InstPtr alloc_inst);
};

/**
 * @brief A pass that inserts phi nodes to make the function in SSA form.
 * However, this pass does not rename variables.
 * @note This pass requires `MemoryToRegisterPass` and
 * `FillDominanceFrontierPass` to be run before.
 * @warning This pass will only break use relationship in use-def, so a temp can
 * find its def, but cannot find its use.
 */
class PhiInsertingPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;
};

/**
 * @brief A pass that renames variables to make the function in SSA form.
 * @note This pass requires `PhiInsertingPass` to be run before.
 * @warning This pass will break use-def relationship filled by `FillUsesPass`.
 */
class VariableRenamingPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;

private:
    using RenameStack =
        std::unordered_map<ir::TempPtr, std::stack<ir::TempPtr>>;
    void _dom_tree_preorder_traversal(ir::BlockPtr block,
                                      RenameStack &rename_stack,
                                      uint &temp_counter);

    ir::TempPtr _create_temp_from(ir::TempPtr old_temp, uint &temp_counter);
};

/**
 * @brief A pass that constructs SSA form for a function.
 * @note This pass requires `FillUsesPass` and `FillDominanceFrontierPass` to be
 * run before.
 * @note This pass will break use-def relationship filled by `FillUsesPass`.
 */
using SSAConstructPass =
    PassPipeline<opt::MemoryToRegisterPass, opt::PhiInsertingPass,
                 opt::VariableRenamingPass>;

/**
 * @brief A pass that destructs SSA form for a function.
 * @note This pass requires `FillPredsPass` to be run before.
 * @note This pass will break use-def relationship filled by `FillUsesPass`.
 */
class SSADestructPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;

private:
    using ParallelCopy = std::vector<std::pair<ir::ValuePtr, ir::ValuePtr>>;
    using ParallelCopyMap = std::unordered_map<ir::BlockPtr, ParallelCopy>;

    void _split_critical_edge(const std::vector<ir::BlockPtr> &blocks,
                              ParallelCopyMap &parallel_copy_map,
                              uint *block_counter_ptr);
    void _parallel_copy_to_sequential(ParallelCopyMap &parallel_copy_map,
                                      uint &temp_counter);
    void _emit_copy(ir::BlockPtr block, ir::ValuePtr to, ir::ValuePtr from);
    void _update_phis(std::vector<ir::PhiPtr> &phis, ir::BlockPtr old_block,
                      ir::BlockPtr new_block);
};

} // namespace opt
