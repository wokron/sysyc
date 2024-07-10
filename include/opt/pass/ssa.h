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

} // namespace opt
