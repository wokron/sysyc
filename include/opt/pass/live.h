#pragma once

#include "opt/pass/base.h"

namespace opt {

/**
 * @brief A pass that performs liveness analysis on a function.
 * @note This pass requires `ReversePostOrderPass` to be run before.
 */
class LivenessAnalysisPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;

private:
    void _init_live_use_def(ir::Block &block);
    bool _update_live(ir::Block &block);
};

/**
 * @brief A pass that finds the live intervals of each temp in a function.
 * @note This pass requires `LivenessAnalysisPass` and `FillUsePass` to be run before.
 */
class FillIntervalPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;

private:
    void _find_intervals_in_block(
        ir::Block &block, std::unordered_map<ir::TempPtr, int> &first_def,
        std::unordered_map<ir::TempPtr, int> &last_use, int &number);
};

} // namespace opt
