#pragma once

#include "opt/pass/base.h"
#include <unordered_set>

namespace opt {


/**
 * @brief Pass to fill predecessors of blocks.
 * @note Nothing is required before this pass.
 */
class FillPredsPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;
};

/**
 * @brief Pass to fill use-def relationship.
 * @note Nothing is required before this pass.
 */
class FillUsesPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;
};

/**
 * @brief Pass to fill reverse post order of blocks.
 * @note Nothing is required before this pass.
 */
class FillReversePostOrderPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;

private:
    static void _post_order_traverse(ir::BlockPtr block,
                                     std::unordered_set<ir::BlockPtr> &visited,
                                     std::vector<ir::BlockPtr> &post_order);
};

/**
 * @brief Pass to fill dominators of blocks.
 * @note Requires `FillReversePostOrderPass` and `FillPredsPass`.
 */
class CooperFillDominatorsPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;

private:
    ir::BlockPtr _intersect(ir::BlockPtr b1, ir::BlockPtr b2);
};

/**
 * @brief Pass to fill dominance frontier of blocks.
 * @note Requires `CooperFillDominatorsPass`.
 */
class FillDominanceFrontierPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;

private:
    bool _is_strictly_dominate(ir::BlockPtr b1, ir::BlockPtr b2);
};

} // namespace opt
