#pragma once

#include "opt/pass/base.h"
#include "ir/folder.h"

namespace opt {

class ConstAndCopyPropagationPass : public BasicBlockPass {
  public:
    bool run_on_basic_block(ir::Block &block) override;

  private:
    ir::ValuePtr _fold_if_can(const ir::Inst &inst);

    ir::Folder _folder;
};

} // namespace opt
