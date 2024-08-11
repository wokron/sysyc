#include "opt/pass/loop.h"
#include <algorithm>
#include <queue>
#include <unordered_set>

namespace opt {

static std::unordered_set<ir::InstType> non_invariant_insts = {
    ir::InstType::IPAR,   ir::InstType::IARG,   ir::InstType::ICALL,
    ir::InstType::ICEQS,  ir::InstType::ICNES,  ir::InstType::ICLES,
    ir::InstType::ICLTS,  ir::InstType::ICGES,  ir::InstType::ICGTS,
    ir::InstType::ICEQW,  ir::InstType::ICNEW,  ir::InstType::ICSLEW,
    ir::InstType::ICSLTW, ir::InstType::ICSGEW, ir::InstType::ICSGTW,
    ir::InstType::ILOADL, ir::InstType::ILOADW, ir::InstType::ILOADS};

static std::unordered_set<ir::InstType> aggresive_insts = {
    ir::InstType::ISTOREL, ir::InstType::ISTOREW, ir::InstType::ISTORES};

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
    ir::InstPtr inst, ir::BlockPtr block,
    const std::unordered_set<ir::BlockPtr> &loop_blocks,
    const std::unordered_set<ir::InstPtr> &invariants, bool aggresive) {

    if (in(non_invariant_insts, inst->insttype)) {
        return false;
    }
    if ((!aggresive) && in(aggresive_insts, inst->insttype)) {
        return false;
    }

    std::vector<ir::ValuePtr> candidates = {inst->to, inst->arg[0],
                                            inst->arg[1]};
    ir::InstPtr inside_def_ins = nullptr;

    bool first = true;
    for (auto value : candidates) {
        if (!value) {
            continue;
        }
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
            if (first && (def_ins == inst)) {
                first = false;
                continue;
            }
            auto def_blk = inst_def->blk;
            if (in(loop_blocks, def_blk)) {
                if (inside_def_ins) {
                    return false; // require exact one reaching definition
                }
                inside_def_ins = def_ins;
            }

            if (def_blk == block) {
                for (auto ins : block->insts) {
                    if (ins == inst) {
                        // if are the same inst, return false first
                        return false;
                    }
                    if (ins == def_ins) {
                        break; // def must be before inst
                    }
                }
            }
        }
    }

    return ((!inside_def_ins) || in(invariants, inside_def_ins));
}

static void insert_before(ir::Function &func, ir::BlockPtr before,
                          ir::BlockPtr block) {
    for (auto blk = func.start; blk; blk = blk->next) {
        if (blk->next == before) {
            blk->next = block;
            block->next = before;
            break;
        }
    }
}

static ir::BlockPtr insert_pre_header(ir::Function &func, ir::BlockPtr header,
                                      ir::BlockPtr body, ir::BlockPtr back) {
    auto pre_header = std::shared_ptr<ir::Block>(
        new ir::Block{(*func.block_counter_ptr)++, "pre_header"});

    insert_before(func, header, pre_header);

    // duplicate the header block if it can jumps outside
    if (header->jump.type != ir::Jump::JMP) {
        auto decoy = std::shared_ptr<ir::Block>(
            new ir::Block{(*func.block_counter_ptr)++, "decoy"});

        insert_before(func, pre_header, decoy);

        decoy->phis = header->phis;
        decoy->insts = header->insts;
        decoy->jump = header->jump;
        decoy->doms.push_back(pre_header);
        decoy->indoms = header->indoms;
        decoy->indoms.push_back(pre_header);

        for (auto pred : header->preds) {
            if (pred == back) {
                continue;
            }
            decoy->preds.push_back(pred);
            if (pred->jump.blk[0] == header) {
                pred->jump.blk[0] = decoy;
            }
            if (pred->jump.blk[1] == header) {
                pred->jump.blk[1] = decoy;
            }
            if (in(pred->doms, header)) {
                std::remove(pred->doms.begin(), pred->doms.end(), header);
                pred->doms.push_back(decoy);
                pred->indoms.push_back(decoy);
            }
        }

        if (decoy->jump.blk[0] == body) {
            decoy->jump.blk[0] = pre_header;
        }
        if (decoy->jump.blk[1] == body) {
            decoy->jump.blk[1] = pre_header;
        }
        pre_header->jump = {ir::Jump::JMP, nullptr, {body, nullptr}};

        if (in(header->doms, body)) {
            std::remove(header->doms.begin(), header->doms.end(), body);
            pre_header->doms.push_back(body);
        }
        body->preds.push_back(pre_header);
    } else {
        for (auto pred : header->preds) {
            if (pred == back) {
                continue;
            }
            if (pred->jump.blk[0] == header) {
                pred->jump.blk[0] = pre_header;
            }
            if (pred->jump.blk[1] == header) {
                pred->jump.blk[1] = pre_header;
            }

            if (in(pred->doms, header)) {
                std::remove(pred->doms.begin(), pred->doms.end(), header);
                pred->doms.push_back(pre_header);
                pred->indoms.push_back(pre_header);
            }
        }
        pre_header->jump = {ir::Jump::JMP, nullptr, {header, nullptr}};

        pre_header->doms.push_back(header);
        pre_header->indoms = header->indoms;
        pre_header->indoms.push_back(header);
    }

    return pre_header;
}

static bool is_nested(ir::BlockPtr block, LicmPass::BackEdge &outer,
                      const std::vector<LicmPass::BackEdge> &back_edges) {
    for (auto back_edge : back_edges) {
        // if wraps around the outer loop
        if (in(back_edge.second->doms, outer.second) &&
            in(outer.first->doms, back_edge.first)) {
            continue;
        }

        // if the block is in the inner loop
        if (in(back_edge.second->doms, block) &&
            in(block->doms, back_edge.first)) {
            return true;
        }
    }

    return false;
}

static bool dominates_uses(ir::InstPtr inst, ir::BlockPtr block) {
    if (inst->to) {
        for (auto use : inst->to->uses) {
            if (auto inst_use = std::get_if<ir::InstUse>(&use)) {
                if (!in(block->indoms, inst_use->blk)) {
                    return false;
                }
            }
        }
    }

    return true;
}

static bool dominates_blocks(ir::BlockPtr block,
                             const std::unordered_set<ir::BlockPtr> &blocks) {
    for (auto blk : blocks) {
        if (!in(block->indoms, blk)) {
            return false;
        }
    }

    return true;
}

static bool is_safe_to_hoist(ir::InstPtr inst, ir::BlockPtr block,
                             const std::unordered_set<ir::InstPtr> &invariant,
                             const std::unordered_set<ir::BlockPtr> &exits) {
    if (!in(invariant, inst)) {
        return false;
    }

    if (!dominates_uses(inst, block)) {
        return false;
    }

    std::unordered_set<ir::BlockPtr> liveout;
    for (auto exit : exits) {
        if (in(exit->live_out, inst->to)) {
            liveout.insert(exit);
        }
    }
    if (!dominates_blocks(block, liveout)) {
        return false;
    }

    return true;
}

bool FillIndirectDominatePass::run_on_function(ir::Function &func) {
    for (auto block = func.start; block; block = block->next) {
        block->indoms = find_dominates(block);
    }
    return true;
}

bool LicmPass::run_on_function(ir::Function &func) {
    auto back_edges = _find_back_edges(func);

    for (auto &back_edge : back_edges) {
        _move_invariant(func, back_edge, back_edges);
    }

    return true;
}

std::vector<LicmPass::BackEdge> LicmPass::_find_back_edges(ir::Function &func) {
    std::vector<BackEdge> back_edges;
    for (auto block = func.start; block; block = block->next) {
        for (auto dom : block->indoms) {
            if (in(block->preds, dom)) {
                back_edges.push_back({dom, block});
            }
        }
    }

    auto forward = [](const BackEdge &a, const BackEdge &b) {
        return a.second->rpo_id < b.second->rpo_id;
    };
    std::sort(back_edges.begin(), back_edges.end(), forward);

    return back_edges;
}

std::unordered_set<ir::InstPtr> LicmPass::_find_loop_invariants(
    const Loop &loop, const std::unordered_set<ir::BlockPtr> &loop_blocks,
    bool aggresive) {

    std::unordered_set<ir::InstPtr> invariants;

    if (loop.front()->preds.size() > 2) {
        // not a natural loop
        return invariants;
    }

    bool changed;
    int i = 0;
    do {
        changed = false;
        for (auto block : loop) {
            for (auto inst : block->insts) {
                if (in(invariants, inst)) {
                    continue;
                }
                if (is_loop_invariant(inst, block, loop_blocks, invariants,
                                      aggresive)) {
                    invariants.insert(inst);
                    changed = true;
                }
            }
        }
    } while (changed);

    return invariants;
}

bool LicmPass::_move_invariant(ir::Function &func, BackEdge &back_edge,
                               const std::vector<BackEdge> &back_edges) {
    ir::BlockPtr header = back_edge.second;
    ir::BlockPtr back = back_edge.first;
    Loop loop = _fill_loop(header, back);

    std::unordered_set<ir::BlockPtr> loop_blocks;
    int critical_block_cnt = 0;
    for (auto block : loop) {
        loop_blocks.insert(block);
        for (auto inst : block->insts) {
            if (in(aggresive_insts, inst->insttype)) {
                critical_block_cnt++;
                break;
            }
        }
    }

    auto invariants =
        _find_loop_invariants(loop, loop_blocks, critical_block_cnt < 2);
    if (invariants.empty()) {
        return false;
    }

    // create a new block before the loop header
    auto pre_header = insert_pre_header(
        func, header, (loop.size() > 1) ? *std::next(loop.begin()) : header,
        back);

    std::unordered_set<ir::BlockPtr> exits;
    for (auto block : loop) {
        if (block->jump.blk[0]) {
            if (!in(loop_blocks, block->jump.blk[0])) {
                exits.insert(block->jump.blk[0]);
            }
        }
        if (block->jump.blk[1]) {
            if (!in(loop_blocks, block->jump.blk[1])) {
                exits.insert(block->jump.blk[1]);
            }
        }
    }

    for (auto block : loop) {
        if (is_nested(block, back_edge, back_edges)) {
            continue; // skip nested loop
        }
        for (auto it = block->insts.begin(); it != block->insts.end();) {
            auto inst = *it;
            if (is_safe_to_hoist(inst, block, invariants, exits)) {
                pre_header->insts.push_back(inst);
                it = block->insts.erase(it);
            } else {
                ++it;
            }
        }
    }

    return true;
}

LicmPass::Loop LicmPass::_fill_loop(ir::BlockPtr header, ir::BlockPtr back) {
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
