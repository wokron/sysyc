#include "opt/pass/gcm.h"
#include <algorithm>
#include <sstream>

bool opt::GCMPass::run_on_function(ir::Function &func) {
    // find pinned instr
    std::unordered_set<ir::InstPtr> pinned;
    for (auto block = func.start; block; block = block->next) {
        for (auto inst : block->insts) {
            if (is_pinned(inst->insttype)) {
                pinned.insert(inst);
            } else {
                inst->eariliest_block = block;
                inst->latest_block = block;
            }
        }
    }
    dfs_for_depth(func.start, 1);
    schedule_early(func, pinned);
    schedule_late(func, pinned);
}

void opt::GCMPass::dfs_for_depth(ir::BlockPtr &block, int depth) {
    block->dom_depth = depth;
    for (auto block_child : block->doms) {
        dfs_for_depth(block_child, depth + 1);
    }
}

void opt::GCMPass::schedule_early(ir::Function &func,
                                  std::unordered_set<ir::InstPtr> &pinned) {
    std::unordered_set<ir::InstPtr> visited(pinned.begin(), pinned.end());
    for (auto block = func.start; block; block = block->next) {
        for (auto inst : block->insts) {
            if (visited.find(inst) == visited.end()) {
                schedule_early_instr(func, inst, visited);
            } else if (pinned.find(inst) != pinned.end()) {
                for (auto value : inst->arg) {
                    if (auto temp =
                            std::dynamic_pointer_cast<ir::Temp>(value)) {
                        if (auto instdef =
                                std::get_if<ir::InstDef>(&temp->defs[0])) {
                            schedule_early_instr(func, instdef->ins, visited);
                        }
                    }
                }
            }
        }
        for (auto phi : block->phis) {
            for (auto [_, value] : phi->args) {
                if (auto temp = std::dynamic_pointer_cast<ir::Temp>(value)) {
                    if (auto instdef =
                            std::get_if<ir::InstDef>(&temp->defs[0])) {
                        schedule_early_instr(func, instdef->ins, visited);
                    }
                }
            }
        }
    }
}

void opt::GCMPass::schedule_early_instr(
    ir::Function &func, ir::InstPtr &inst,
    std::unordered_set<ir::InstPtr> &visited) {
    if (visited.find(inst) != visited.end()) {
        return;
    }
    visited.insert(inst);
    inst->eariliest_block = func.start;
    for (auto use : inst->to->uses) {
        auto instuse = std::get_if<ir::InstUse>(&use);
        if (instuse != nullptr) {
            // instuse.ins is x
            schedule_early_instr(func, instuse->ins, visited);
            if (inst->eariliest_block->dom_depth <
                instuse->ins->eariliest_block->dom_depth) {
                inst->eariliest_block = instuse->ins->eariliest_block;
            }
        }
    }

    for (auto value : inst->arg) {
        if (auto temp = std::dynamic_pointer_cast<ir::Temp>(value)) {
            if (auto instdef = std::get_if<ir::InstDef>(&temp->defs[0])) {
                schedule_early_instr(func, instdef->ins, visited);
                if (inst->eariliest_block->dom_depth <
                    instdef->ins->eariliest_block->dom_depth) {
                    inst->eariliest_block = instdef->ins->eariliest_block;
                }
            }
        }
    }
}

void opt::GCMPass::schedule_late(ir::Function &func,
                                 std::unordered_set<ir::InstPtr> &pinned) {
    std::unordered_set<ir::InstPtr> visited(pinned.begin(), pinned.end());
    for (auto inst : pinned) {
        for (auto use : inst->to->uses) {
            auto instuse = std::get_if<ir::InstUse>(&use);
            if (instuse != nullptr) {
                schedule_late_instr(func, instuse->ins, visited);
            }
        }
    }
    for (auto block = func.start; block; block = block->next) {
        std::unordered_set<ir::InstPtr> instrs;
        for (auto it = block->insts.rbegin(); it != block->insts.rend(); ++it) {
            instrs.insert(*it);
        }
        for (auto inst : instrs) {
            if (visited.find(inst) == visited.end()) {
                schedule_late_instr(func, inst, visited);
            }
        }
    }
}

void opt::GCMPass::schedule_late_instr(
    ir::Function &func, ir::InstPtr &inst,
    std::unordered_set<ir::InstPtr> &visited) {
    if (visited.find(inst) != visited.end()) {
        return;
    }
    visited.insert(inst);
    ir::BlockPtr lca = nullptr;
    for (auto use : inst->to->uses) {
        if (auto instuse = std::get_if<ir::InstUse>(&use)) {
            schedule_late_instr(func, instuse->ins, visited);
            ir::BlockPtr use_block = instuse->ins->latest_block;
            lca = find_lca(lca, use_block);
        } else if (auto phiuse = std::get_if<ir::PhiUse>(&use)) {
            ir::BlockPtr use_block = nullptr;
            size_t index = 0;
            for (auto [_, value] : phiuse->phi->args) {
                if (auto temp = std::dynamic_pointer_cast<ir::Temp>(value)) {
                    if (auto instdef =
                            std::get_if<ir::InstDef>(&temp->defs[0])) {
                        if (instdef->ins == inst) {
                            use_block = phiuse->blk->preds[index];
                            break;
                        }
                    }
                }
                index++;
            }
            lca = find_lca(lca, use_block);
        }
    }

    // select block
    ir::BlockPtr best = lca;
    while (lca != inst->eariliest_block) {
        if (lca->dom_depth < best->dom_depth) {
            best = lca;
        }
        lca = lca->idom;
    }
    if (lca->dom_depth < best->dom_depth) {
        best = lca;
    }
    inst->latest_block = best;

    // move to the best

    auto instdef = std::get_if<ir::InstDef>(&inst->to->defs[0]);
    ir::BlockPtr block = instdef->blk;
    // remove from the block
    for (auto it = block->insts.begin(); it != best->insts.end(); ++it) {
        if (*it == inst) {
            block->insts.erase(it);
            break;
        }
    }
    // find the position
    auto pos = best->insts.end();
    std::unordered_set<ir::InstPtr> use_inst_set;
    for (auto use : inst->to->uses) {
        if (auto instuse = std::get_if<ir::InstUse>(&use)) {
            use_inst_set.insert(instuse->ins);
        }
    }
    for (auto it = best->insts.begin(); it != best->insts.end(); ++it) {
        if (use_inst_set.find(*it) != use_inst_set.end()) {
            pos = it;
        }
    }
    best->insts.insert(pos, inst);
    // change state? maybe more
    instdef->blk = best;
    // change use block
    for (auto value : inst->arg) {
        if (auto temp = std::dynamic_pointer_cast<ir::Temp>(value)) {
            for (auto use : temp->uses) {
                if (auto instuse = std::get_if<ir::InstUse>(&use)) {
                    if (instuse->ins == inst) {
                        instuse->blk = best;
                        break;
                    }
                }
            }
        }
    }
}

ir::BlockPtr opt::GCMPass::find_lca(ir::BlockPtr block1, ir::BlockPtr block2) {
    if (block1 == nullptr) {
        return block2;
    }
    while (block1->dom_depth < block2->dom_depth) {
        block2 = block2->idom;
    }
    while (block2->dom_depth < block1->dom_depth) {
        block1 = block1->idom;
    }
    while (block1 != block2) {
        block1 = block1->idom;
        block2 = block2->idom;
    }
    return block1;
}

// fix:not sure
bool opt::GCMPass::is_pinned(ir::InstType insttype) {
    switch (insttype) {
    case ir::InstType::IALLOC4:
    case ir::InstType::IALLOC8:
    case ir::InstType::ILOADS:
    case ir::InstType::ILOADL:
    case ir::InstType::ILOADW:
    case ir::InstType::IPAR:
    case ir::InstType::ICALL:
    case ir::InstType::ICOPY:
        return true;
    default:
        return false;
    }
}
