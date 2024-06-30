#pragma once

#include "opt/pass/base.h"

namespace opt {

/**
 * @brief A pass that performs liveness analysis on a function.
 * @note Requires `ReversePostOrderPass` to be run before.
 */
class LivenessAnalysisPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;

private:
    void _init_use_def(ir::Block &block);
    bool _update_live(ir::Block &block);
};

} // namespace opt
