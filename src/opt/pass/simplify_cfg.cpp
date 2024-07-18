#include "opt/pass/simplify_cfg.h"
#include <algorithm>
#include <unordered_set>

namespace opt {

bool EmptyBlockRemovalPass::run_on_function(ir::Function &func) {
    _block_replacement.clear();
    for (auto block = func.start; block; block = block->next) {
        switch (block->jump.type) {
        case ir::Jump::JMP:
            block->jump.blk[0] = _get_replacement(block->jump.blk[0]);
            break;
        case ir::Jump::JNZ:
            block->jump.blk[0] = _get_replacement(block->jump.blk[0]);
            block->jump.blk[1] = _get_replacement(block->jump.blk[1]);
            break;
        case ir::Jump::RET:
            break;
        default:
            throw std::logic_error("invalid jump type");
        }
    }

    return _changed;
}

ir::BlockPtr EmptyBlockRemovalPass::_get_replacement(ir::BlockPtr block) {
    auto it = _block_replacement.find(block);
    if (it != _block_replacement.end()) {
        return it->second;
    }

    // if block is not empty, return itself
    if (block->phis.size() != 0 || block->insts.size() != 0) {
        return _block_replacement[block] = block;
    }

    // if block is empty, but has jnz or ret jump, return itself.
    // otherwise, return the successor block. and since successor block could be
    // empty, we need to recursively find the replacement
    switch (block->jump.type) {
    case ir::Jump::JNZ:
    case ir::Jump::RET:
        return _block_replacement[block] = block;
    case ir::Jump::JMP:
        _changed = true;
        return _block_replacement[block] = _get_replacement(block->jump.blk[0]);
    default:
        throw std::logic_error("invalid jump type");
    }
}

bool UnreachableBlockRemovalPass::run_on_function(ir::Function &func) {
    bool changed = false;

    // calculate the transitive closure of control flow graph
    auto reachable = _find_reachable_blocks(func);

    // remove unreachable blocks
    auto block = func.start;
    while (block->next) {
        if (reachable.count(block->next) == 0) {
            block->next = block->next->next;
            changed = true;
            continue;
        }
        block = block->next;
    }

    return changed;
}

std::unordered_set<ir::BlockPtr>
UnreachableBlockRemovalPass::_find_reachable_blocks(const ir::Function &func) {
    std::unordered_set<ir::BlockPtr> reachable = {func.start};
    _find_reachable_blocks(*func.start, reachable);
    return reachable;
}

void UnreachableBlockRemovalPass::_find_reachable_blocks(
    const ir::Block &block, std::unordered_set<ir::BlockPtr> &reachable) {
    std::vector<ir::BlockPtr> next_blocks;
    switch (block.jump.type) {
    case ir::Jump::JNZ:
        next_blocks.push_back(block.jump.blk[1]);
        // fall through
    case ir::Jump::JMP:
        next_blocks.push_back(block.jump.blk[0]);
        break;
    case ir::Jump::RET:
        break;
    default:
        throw std::logic_error("invalid jump type");
    }

    for (auto next : next_blocks) {
        if (reachable.insert(next).second) {
            _find_reachable_blocks(*next, reachable);
        }
    }
}

bool BlockMergingPass::run_on_function(ir::Function &func) {
    bool changed = false;

    for (auto block = func.start; block; block = block->next) {
        // if the block has only one predecessor
        if (block->preds.size() != 1) {
            continue;
        }

        // and the predecessor has the block as its only successor
        auto pred = block->preds[0];
        if (pred->jump.type == ir::Jump::JNZ) {
            continue;
        } else if (pred->jump.type == ir::Jump::RET) {
            throw std::logic_error(
                "block with return jump type should not have successors");
        }

        if (block->phis.size() != 0) {
            throw std::logic_error("block with phi should not be merged");
        }

        // we can merge the block into its predecessor

        changed = true;

        // merge instructions
        pred->insts.insert(pred->insts.end(), block->insts.begin(),
                           block->insts.end());

        // merge jump
        pred->jump = block->jump;

        // then update predecessors
        switch (block->jump.type) {
        case ir::Jump::JMP: {
            auto it = std::find(block->jump.blk[0]->preds.begin(),
                                block->jump.blk[0]->preds.end(), block);
            *it = pred;
        } break;
        case ir::Jump::JNZ: {
            auto it = std::find(block->jump.blk[0]->preds.begin(),
                                block->jump.blk[0]->preds.end(), block);
            *it = pred;
            if (block->jump.blk[0] != block->jump.blk[1]) {
                it = std::find(block->jump.blk[1]->preds.begin(),
                               block->jump.blk[1]->preds.end(), block);
                *it = pred;
            }
        } break;
        default:
            break;
        }
        block->preds.clear();
        block->jump = {
            .type = ir::Jump::RET,
        };
    }

    return changed;
}

} // namespace opt
