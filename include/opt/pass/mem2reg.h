#pragma once

#include "opt/pass/base.h"
#include "opt/pass/cfg.h"
#include <unordered_set>

namespace opt {
class Mem2regPass : public FunctionPass {
  public:
    bool run_on_function(ir::Function &func) override;

  private:
    void insert_phi(ir::BlockPtr block, ir::InstPtr instruction);
    void rename_dfs(ir::BlockPtr block, ir::InstPtr instruction);
};
} // namespace opt