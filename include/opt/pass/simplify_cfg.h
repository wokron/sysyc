#pragma once

#include "opt/pass/base.h"
#include "opt/pass/cfg.h"
#include <unordered_set>

namespace opt {

/**
 * @brief Pass to remove empty blocks.
 * @note Empty blocks are blocks without any instructions or phi nodes.
 * @note This pass will make empty block unreachable, so they can be removed
 * later by UnreachableBlockRemovalPass.
 * @note Nothing is required before this pass.
 * @warning This pass will break predecessor relationship filled by `FillPredsPass`.
 */
class EmptyBlockRemovalPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;

private:
    ir::BlockPtr _get_replacement(ir::BlockPtr block);

    std::unordered_map<ir::BlockPtr, ir::BlockPtr> _block_replacement;
    bool _changed = false;
};

/**
 * @brief Pass to merge blocks.
 * @note This pass will merge blocks that have only one predecessor and one
 * successor.
 * @note This pass will make some blocks unreachable, so they can be removed
 * later by UnreachableBlockRemovalPass.
 * @note Requires `FillPredsPass` to be run before.
 * @warning This pass will break predecessor relationship filled by `FillPredsPass`.
 */
class BlockMergingPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;
};

/**
 * @brief Pass to remove unreachable blocks.
 * @note This pass will remove blocks that are not reachable
 * from the entry block.
 * @note Nothing is required before this pass.
 */
class UnreachableBlockRemovalPass : public FunctionPass {
public:
    bool run_on_function(ir::Function &func) override;

private:
    static std::unordered_set<ir::BlockPtr>
    _find_reachable_blocks(const ir::Function &func);

    static void
    _find_reachable_blocks(const ir::Block &block,
                           std::unordered_set<ir::BlockPtr> &reachable);
};

/**
 * @brief Pass to simplify CFG.
 * @note This pass will remove empty blocks, merge blocks, and remove
 * unreachable blocks.
 * @note Requires `FillPredsPass` to be run before.
 * @warning This pass will break every CFG-related passes.
 */
using SimplifyCFGPass =
    PassPipeline<BlockMergingPass, EmptyBlockRemovalPass,
                 UnreachableBlockRemovalPass>;

} // namespace opt
