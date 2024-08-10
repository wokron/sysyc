#pragma once

#include "opt/pass/base.h"
#include "opt/pass/cfg.h"
#include <list>
#include <utility>
#include <vector>

namespace opt {

/**
 * @brief It will fill all indirect dominate blocks for each block.
 * @note This pass requires `FillDominanceFrontierPass`, and Mem2Reg must be
 * done.
 */
class FillIndirectDominatePass : public FunctionPass {
  public:
    bool run_on_function(ir::Function &func) override;
};

/**
 * @brief A pass that performs loop invariant code motion on a function.
 * @note This pass requires `FillIndirectDominatePass`,
 * `LivenessAnalysisPass` and `FillUsesPass`.
 */
class LicmPass : public FunctionPass {
  public:
    using BackEdge = std::pair<ir::BlockPtr, ir::BlockPtr>;
    using Loop = std::list<ir::BlockPtr>;

    bool run_on_function(ir::Function &func) override;

  private:
    std::vector<BackEdge> _find_back_edges(ir::Function &func);
    std::unordered_set<ir::InstPtr>
    _find_loop_invariants(const Loop &loop,
                          const std::unordered_set<ir::BlockPtr> &loop_blocks,
                          bool aggresive);
    bool _move_invariant(ir::Function &func, BackEdge &back_edge,
                         const std::vector<BackEdge> &back_edges);
    Loop _fill_loop(ir::BlockPtr header, ir::BlockPtr back);
};

using LoopInvariantCodeMotionPass =
    PassPipeline<FillIndirectDominatePass, LicmPass>;

} // namespace opt
