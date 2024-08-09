#pragma once

#include "opt/pass/base.h"
#include "opt/pass/cfg.h"
#include <list>
#include <utility>
#include <vector>

namespace opt {

/**
 * @brief A pass that performs loop invariant code motion on a function.
 * @note This pass requires `FillDominanceFrontierPass`, and Mem2Reg must be
 * done.
 */
class LoopInvariantCodeMotionPass : public FunctionPass {
  public:
    using BackEdge = std::pair<ir::BlockPtr, ir::BlockPtr>;
    using Loop = std::list<ir::BlockPtr>;

    bool run_on_function(ir::Function &func) override;

  private:
    std::vector<BackEdge> _find_back_edges(ir::Function &func);
    bool _move_invariant(ir::Function &func, BackEdge &back_edge);
    Loop _fill_loop(ir::BlockPtr header, ir::BlockPtr back);
};

} // namespace opt
