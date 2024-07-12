#pragma once

#include "opt/pass/base.h"
#include <stack>

namespace opt {

class MemoryToRegisterPass : public FunctionPass {
  public:
    bool run_on_function(ir::Function &func) override;

  private:
    bool _mem_to_reg(ir::InstPtr alloc_inst);
    bool _is_able_to_reg(ir::InstPtr alloc_inst);
};

class PhiInsertingPass : public FunctionPass {
  public:
    bool run_on_function(ir::Function &func) override;
};

class VariableRenamingPass : public FunctionPass {
  public:
    bool run_on_function(ir::Function &func) override;

  private:
    using RenameStack =
        std::unordered_map<ir::TempPtr, std::stack<ir::TempPtr>>;
    void _dom_tree_preorder_traversal(ir::BlockPtr block,
                                      RenameStack &rename_stack, uint &temp_counter);

    ir::TempPtr _create_temp_from(ir::TempPtr old_temp, uint &temp_counter);
};

using SSAConstructPass = PassPipeline<opt::MemoryToRegisterPass, opt::PhiInsertingPass, opt::VariableRenamingPass>;

class SSADestructPass : public FunctionPass {
  public:
    bool run_on_function(ir::Function &func) override;

  private:
    using ParallelCopy = std::vector<std::pair<ir::ValuePtr, ir::ValuePtr>>;
    using ParallelCopyMap = std::unordered_map<ir::BlockPtr, ParallelCopy>;

    void _split_critical_edge(const std::vector<ir::BlockPtr> &blocks, ParallelCopyMap &parallel_copy_map, uint *block_counter_ptr);
    void _parallel_copy_to_sequential(ParallelCopyMap &parallel_copy_map, uint &temp_counter);
    void _emit_copy(ir::BlockPtr block, ir::ValuePtr to, ir::ValuePtr from);
    void _update_phis(std::vector<ir::PhiPtr> &phis, ir::BlockPtr old_block, ir::BlockPtr new_block);

};

} // namespace opt
