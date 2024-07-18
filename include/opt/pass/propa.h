#pragma once

#include "opt/pass/base.h"
#include "ir/folder.h"

namespace opt {

/**
 * @brief A pass that performs constant and copy propagation locally.
 * @note Nothing is required before this pass.
 * @warning This pass will break use-def relationship filled by `FillUsesPass`
 */
class LocalConstAndCopyPropagationPass : public BasicBlockPass {
  public:
    bool run_on_basic_block(ir::Block &block) override;

  private:
    ir::ValuePtr _fold_if_can(const ir::Inst &inst);

    ir::Folder _folder;
};

/**
 * @brief A pass that performs copy propagation locally.
 * @note This pass requires `FillUsesPass` and `SSAConstructPass` to be run before.
 * @warning This pass will break use-def relationship filled by `FillUsesPass`
 */
class CopyPropagationPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;
};

} // namespace opt
