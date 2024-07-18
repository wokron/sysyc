#include "opt/pass/dead.h"
#include <algorithm>

bool opt::SimpleDeadCodeEliminationPass::run_on_function(ir::Function &func) {
    _frontier.clear();
    // init marked
    for (auto block = func.start; block; block = block->next) {
        for (auto phi : block->phis) {
            phi->marked = false;
        }
        for (auto inst : block->insts) {
            inst->marked = false;
        }
    }

    _mark_always_alive(func);
    _mark_all_alive(func);
    return _remove_unmarked(func);
}

void opt::SimpleDeadCodeEliminationPass::_mark_always_alive(
    ir::Function &func) {
    for (auto block = func.start; block; block = block->next) {
        for (auto inst : block->insts) {
            if (inst->insttype == ir::InstType::ISTOREL ||
                inst->insttype == ir::InstType::ISTORES ||
                inst->insttype == ir::InstType::ISTOREW) {
                inst->marked = true;
                _insert_if_temp(_frontier, inst->arg[0]);
                _insert_if_temp(_frontier, inst->arg[1]);
            } else if (inst->insttype == ir::InstType::IPAR) {
                inst->marked = true;
            } else if (inst->insttype == ir::InstType::IARG) {
                _insert_if_temp(_frontier, inst->arg[0]);
                inst->marked = true;
            } else if (inst->insttype == ir::InstType::ICALL) {
                inst->marked = true;
            }
        }
        // jump insts are always alive
        if (block->jump.type == ir::Jump::JNZ ||
            block->jump.type == ir::Jump::RET) {
            _insert_if_temp(_frontier, block->jump.arg);
        }
    }
}

void opt::SimpleDeadCodeEliminationPass::_mark_all_alive(ir::Function &func) {
    while (!_frontier.empty()) {
        std::unordered_set<ir::TempPtr> new_frontier;
        for (auto frontier_temp : _frontier) {
            for (auto def : frontier_temp->defs) {
                if (auto instdef = std::get_if<ir::InstDef>(&def)) {
                    if (instdef->ins->marked) {
                        continue;
                    }
                    instdef->ins->marked = true;
                    _insert_if_temp(new_frontier, instdef->ins->arg[0]);
                    _insert_if_temp(new_frontier, instdef->ins->arg[1]);
                } else if (auto phidef = std::get_if<ir::PhiDef>(&def)) {
                    if (phidef->phi->marked) {
                        continue;
                    }
                    phidef->phi->marked = true;
                    for (auto [block, value] : phidef->phi->args) {
                        _insert_if_temp(new_frontier, value);
                    }
                }
            }
        }
        _frontier = new_frontier;
    }
}

bool opt::SimpleDeadCodeEliminationPass::_remove_unmarked(ir::Function &func) {
    bool changed = false;
    for (auto block = func.start; block; block = block->next) {
        auto it1 =
            std::remove_if(block->phis.begin(), block->phis.end(),
                           [](const ir::PhiPtr &phi) { return !phi->marked; });
        changed += it1 != block->phis.end();
        block->phis.erase(it1, block->phis.end());

        auto it2 = std::remove_if(
            block->insts.begin(), block->insts.end(),
            [](const ir::InstPtr &inst) { return !inst->marked; });
        changed |= it2 != block->insts.end();
        block->insts.erase(it2, block->insts.end());
    }

    return changed;
}

void opt::SimpleDeadCodeEliminationPass::_insert_if_temp(
    std::unordered_set<ir::TempPtr> &set, ir::ValuePtr value) {
    if (auto temp = std::dynamic_pointer_cast<ir::Temp>(value)) {
        set.insert(temp);
    }
}
