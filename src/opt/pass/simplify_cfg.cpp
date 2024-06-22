#include "opt/pass/simplify_cfg.h"
#include <algorithm>
#include <unordered_set>

namespace opt {

bool EmptyBlockRemovalPass::run_on_function(ir::Function &func) {
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

bool BlockMergingPass::run_on_function(ir::Function &func) {
    _init_all_predecessors(func);

    return _merge_blocks(func);
}

void BlockMergingPass::_init_all_predecessors(ir::Function &func) {
    for (auto block = func.start; block; block = block->next) {
        _insert_predecessor(block);
    }
}

void BlockMergingPass::erase_by_kv(
    std::unordered_multimap<ir::BlockPtr, ir::BlockPtr> &map, ir::BlockPtr key,
    ir::BlockPtr value) {
    auto [from, to] = map.equal_range(key);
    auto it = std::find_if(
        from, to, [value](const auto &kv) { return kv.second == value; });
    if (it != map.end()) {
        map.erase(it);
    }
}

void BlockMergingPass::_remove_predecessor(ir::BlockPtr block) {
    switch (block->jump.type) {
    case ir::Jump::JNZ:
        erase_by_kv(_predecessors, block->jump.blk[1], block);
        // fall through
    case ir::Jump::JMP:
        erase_by_kv(_predecessors, block->jump.blk[0], block);
        break;
    case ir::Jump::RET:
        break;
    default:
        throw std::logic_error("invalid jump type");
    }
}

void BlockMergingPass::_insert_predecessor(ir::BlockPtr block) {
    switch (block->jump.type) {
    case ir::Jump::JNZ:
        _predecessors.insert({block->jump.blk[1], block});
        // fall through
    case ir::Jump::JMP:
        _predecessors.insert({block->jump.blk[0], block});
        break;
    case ir::Jump::RET:
        break;
    default:
        throw std::logic_error("invalid jump type");
    }
}

bool BlockMergingPass::_merge_blocks(ir::Function &func) {
    bool changed = false;

    for (auto block = func.start; block; block = block->next) {
        // get predecessors, only one predecessor is allowed
        auto range = _predecessors.equal_range(block);
        if (std::distance(range.first, range.second) != 1) {
            continue;
        }
        auto pred = range.first->second;
        if (pred->jump.type == ir::Jump::JNZ) {
            continue;
        } else if (pred->jump.type == ir::Jump::RET) {
            throw std::logic_error(
                "block with return jump type should not have successors");
        }

        // remove block from predecessors
        _predecessors.erase(range.first);

        if (block->phis.size() != 0) {
            throw std::logic_error("block with phi should not be merged");
        }

        // merge instructions
        pred->insts.insert(pred->insts.end(), block->insts.begin(),
                           block->insts.end());

        // merge jump
        pred->jump = block->jump;

        // then update predecessors of successors
        _remove_predecessor(block);
        _insert_predecessor(pred);

        changed = true;
    }

    return changed;
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

} // namespace opt
