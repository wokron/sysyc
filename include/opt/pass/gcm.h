#pragma once

#include "ir/folder.h"
#include "opt/pass/base.h"

namespace opt {

class GCMPass : public FunctionPass {
  public:
    bool run_on_function(ir::Function &func) override;
    void dfs_for_depth(ir::BlockPtr &block, int depth);
    void schedule_early(ir::Function &func,
                        std::unordered_set<ir::InstPtr> &pinned);
    void schedule_early_instr(ir::Function &func, ir::InstPtr &inst,
                              std::unordered_set<ir::InstPtr> &visited);
    void schedule_late(ir::Function &func,
                       std::unordered_set<ir::InstPtr> &pinned);
    void schedule_late_instr(ir::Function &func, ir::InstPtr &inst,
                             std::unordered_set<ir::InstPtr> &visited);
    ir::BlockPtr find_lca(ir::BlockPtr block1, ir::BlockPtr block2);                         
    bool is_pinned(ir::InstType insttype);
};
} // namespace opt