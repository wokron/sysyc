#include "opt/pass/loop.h"
#include <algorithm>
#include <queue>
#include <unordered_set>

namespace opt {

template <typename T> static bool in(const std::vector<T> &container, T &elem) {
    return std::find(container.begin(), container.end(), elem) !=
           container.end();
}

template <typename T>
static bool in(const std::unordered_set<T> &container, T &elem) {
    return std::find(container.begin(), container.end(), elem) !=
           container.end();
}

std::vector<ir::BlockPtr> static find_dominates(ir::BlockPtr block) {
    std::vector<ir::BlockPtr> dominates;
    std::queue<ir::BlockPtr> queue;
    std::unordered_set<ir::BlockPtr> visited;

    queue.push(block);
    while (!queue.empty()) {
        auto block = queue.front();
        queue.pop();
        if (in(visited, block)) {
            continue;
        }
        visited.insert(block);

        dominates.push_back(block);
        for (auto dom : block->doms) {
            queue.push(dom);
        }
    }

    return dominates;
}

// 1. All reaching definitions of var are outside the loop.
// 2. There is exactly one reaching definition of var and the definition is
// loop-invariant.
bool static is_loop_invariant(
    ir::InstPtr inst, const std::unordered_set<ir::BlockPtr> &loop_blocks,
    const std::unordered_set<ir::InstPtr> &invariants) {

    static std::unordered_set<ir::InstType> non_invariant_insts = {
        ir::InstType::IPAR,   ir::InstType::IARG,   ir::InstType::ICALL,
        ir::InstType::ICEQS,  ir::InstType::ICNES,  ir::InstType::ICLES,
        ir::InstType::ICLTS,  ir::InstType::ICGES,  ir::InstType::ICGTS,
        ir::InstType::ICEQW,  ir::InstType::ICNEW,  ir::InstType::ICSLEW,
        ir::InstType::ICSLTW, ir::InstType::ICSGEW, ir::InstType::ICSGTW};

    if (in(non_invariant_insts, inst->insttype)) {
        return false;
    }

    std::vector<ir::ValuePtr> candidates = {inst->to, inst->arg[0],
                                            inst->arg[1]};
    ir::InstPtr inside_def_ins = nullptr;

    for (auto value : candidates) {
        auto temp = std::dynamic_pointer_cast<ir::Temp>(value);
        if (!temp) {
            continue;
        }
        for (auto def : temp->defs) {
            auto inst_def = std::get_if<ir::InstDef>(&def);
            if (inst_def == nullptr) {
                // skip PhiDef for now
                return false;
            }

            auto def_ins = inst_def->ins;
            if ((def_ins == inst) && (value == inst->to)) {
                continue;
            }
            auto def_blk = inst_def->blk;
            if (in(loop_blocks, def_blk)) {
                if (inside_def_ins) {
                    return false; // require exact one reaching definition
                }
                inside_def_ins = def_ins;
            }
        }
    }
    if ((!inside_def_ins) || in(invariants, inside_def_ins)) {
        return true;
    }

    return false;
}

std::unordered_set<ir::InstPtr> static find_loop_invariants(
    const LoopInvariantCodeMotionPass::Loop &loop) {
    std::unordered_set<ir::InstPtr> invariants;
    std::unordered_set<ir::BlockPtr> loop_blocks;

    for (auto block : loop) {
        loop_blocks.insert(block);
    }

    bool changed;
    do {
        changed = false;
        for (auto block : loop) {
            for (auto inst : block->insts) {
                if (in(invariants, inst)) {
                    continue;
                }
                if (is_loop_invariant(inst, loop_blocks, invariants)) {
                    invariants.insert(inst);
                    changed = true;
                }
            }
        }
    } while (changed);

    return invariants;
}

static ir::BlockPtr insert_pre_header(ir::Function &func, ir::BlockPtr header) {
    auto pre_header = std::shared_ptr<ir::Block>(
        new ir::Block{(*func.block_counter_ptr)++, "pre_header"});

    for (auto block = func.start; block; block = block->next) {
        if (block->next == header) {
            block->next = pre_header;
            pre_header->next = header;
            break;
        }
    }

    for (auto pred : header->preds) {
        pre_header->preds.push_back(pred);
        if (pred->jump.blk[0] == header) {
            pred->jump.blk[0] = pre_header;
        }
        if (pred->jump.blk[1] == header) {
            pred->jump.blk[1] = pre_header;
        }

        // update dominance to keep the loop structure
        // but it still destroys the original CFG
        for (auto &dom : pred->doms) {
            if (dom == header) {
                dom = pre_header;
            }
        }
    }
    header->preds.clear();
    header->preds.push_back(pre_header);

    return pre_header;
}

bool LoopInvariantCodeMotionPass::run_on_function(ir::Function &func) {
    auto back_edges = _find_back_edges(func);

    for (auto &back_edge : back_edges) {
        _move_invariant(func, back_edge);
    }

    return true;
}

std::vector<LoopInvariantCodeMotionPass::BackEdge>
LoopInvariantCodeMotionPass::_find_back_edges(ir::Function &func) {
    std::vector<BackEdge> back_edges;
    for (auto block = func.start; block; block = block->next) {
        if (block->preds.size() > 2) {
            continue; // avoid shared-header loops
        }

        auto dominates = find_dominates(block);
        for (auto dom : dominates) {
            if (in(block->preds, dom)) {
                back_edges.push_back({dom, block});
            }
        }
    }
    return back_edges;
}

bool LoopInvariantCodeMotionPass::_move_invariant(ir::Function &func,
                                                  BackEdge &back_edge) {
    ir::BlockPtr header = back_edge.second;
    ir::BlockPtr back = back_edge.first;
    Loop loop = _fill_loop(header, back);

    auto invariants = find_loop_invariants(loop);
    if (invariants.empty()) {
        return false;
    }
    
    // create a new block before the loop header
    auto pre_header = insert_pre_header(func, header);
    for (auto block : loop) {
        for (auto it = block->insts.begin(); it != block->insts.end();) {
            if (in(invariants, *it)) {
                pre_header->insts.push_back(*it);
                it = block->insts.erase(it);
            } else {
                ++it;
            }
        }
    }
    pre_header->jump.type = ir::Jump::JMP;
    pre_header->jump.blk[0] = header;

    return true;
}

LoopInvariantCodeMotionPass::Loop
LoopInvariantCodeMotionPass::_fill_loop(ir::BlockPtr header,
                                        ir::BlockPtr back) {
    Loop loop;
    std::queue<ir::BlockPtr> queue;
    std::unordered_set<ir::BlockPtr> loop_blocks;
    std::unordered_set<ir::BlockPtr> visited;

    queue.push(back);
    while (!queue.empty()) {
        auto block = queue.front();
        queue.pop();
        if (in(loop_blocks, block)) {
            continue;
        }
        loop_blocks.insert(block);
        if (block == header) {
            continue;
        }
        for (auto pred : block->preds) {
            queue.push(pred);
        }
    }

    queue.push(header);
    while (!queue.empty()) {
        auto block = queue.front();
        queue.pop();
        if (in(visited, block)) {
            continue;
        }
        visited.insert(block);
        loop.push_back(block);
        for (auto dom : block->doms) {
            if ((dom != back) && in(loop_blocks, dom)) {
                queue.push(dom);
            }
        }
    }
    if (back != header) {
        loop.push_back(back);
    }

    return loop;
}

} // namespace opt
