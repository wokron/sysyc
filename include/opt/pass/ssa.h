#pragma once

#include "opt/pass/base.h"

namespace opt {

class MemoryToRegisterPass : public FunctionPass {
  public:
    bool run_on_function(ir::Function &func) override;

    private:
    bool _mem_to_reg(ir::InstPtr alloc_inst);
    bool _is_able_to_reg(ir::InstPtr alloc_inst);
};

} // namespace opt
