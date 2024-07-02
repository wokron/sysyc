#pragma once

#include "opt/pass/base.h"
#include <unordered_set>

namespace opt {

class FillPredsPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;
};

class FillUsesPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;
};

class FillReversePostOrderPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;

private:
    static void _post_order_traverse(ir::BlockPtr block,
                                     std::unordered_set<ir::BlockPtr> &visited,
                                     std::vector<ir::BlockPtr> &post_order);
};

} // namespace opt