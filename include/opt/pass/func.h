#pragma once

#include "opt/pass/base.h"

namespace opt {

/**
 * @brief A pass that fills the `is_leaf` field of each function.
 * A function is a leaf function if it does not call any other functions.
 * @note Nothing is required before this pass.
 */
class FillLeafPass : public ModulePass {
public:
    bool run_on_module(ir::Module &module) override;

private:
    bool _is_leaf_function(ir::Function &func);
};

} // namespace opt
