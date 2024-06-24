#pragma once

#include "opt/pass/base.h"

namespace opt {

class FillPredsPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;
};

class FillUsesPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;
};

} // namespace opt
