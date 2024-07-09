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

class CooperFillDominatorsPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;

private:
    ir::BlockPtr _intersect(ir::BlockPtr b1, ir::BlockPtr b2);
};

class FillDominanceFrontierPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;

private:
    bool _is_strictly_dominate(ir::BlockPtr b1, ir::BlockPtr b2);
};

} // namespace opt
