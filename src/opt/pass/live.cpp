#include "opt/pass/live.h"
#include <algorithm>
#include <limits>

namespace opt {

bool opt::LivenessAnalysisPass::run_on_function(ir::Function &func) {
    for (auto block : func.rpo) {
        block->live_def.clear();
        block->live_in.clear();
        block->live_out.clear();
        _init_use_def(*block);
    }

    bool improved = false;
    do {
        improved = false;
        // iterate in post order (important!)
        for (auto it = func.rpo.rbegin(); it != func.rpo.rend(); ++it) {
            improved |= _update_live(**it);
        }
    } while (improved);

    return false;
}

void LivenessAnalysisPass::_init_use_def(ir::Block &block) {
    // use set is useless
    auto &live_use = block.live_in;

    // phis
    for (auto phi : block.phis) {
        for (auto [_, value] : phi->args) {
            if (auto temp = std::dynamic_pointer_cast<ir::Temp>(value)) {
                if (block.live_def.find(temp) ==
                    block.live_def.end()) { // if not def, then use
                    live_use.insert(temp);
                }
            }
        }
        if (live_use.find(phi->to) == live_use.end()) { // if not use, then def
            block.live_def.insert(phi->to);
        }
    }

    // insts
    for (auto inst : block.insts) {
        if (auto temp = std::dynamic_pointer_cast<ir::Temp>(inst->arg[0])) {
            if (block.live_def.find(temp) ==
                block.live_def.end()) { // if not def, then use
                live_use.insert(temp);
            }
        }
        if (auto temp = std::dynamic_pointer_cast<ir::Temp>(
                inst->arg[1])) { // if not def, then use
            if (block.live_def.find(temp) == block.live_def.end()) {
                live_use.insert(temp);
            }
        }
        if (inst->to) {
            if (live_use.find(inst->to) ==
                live_use.end()) { // if not use, then def
                block.live_def.insert(inst->to);
            }
        }
    }

    // jump
    if (block.jump.type == ir::Jump::JNZ || block.jump.type == ir::Jump::RET) {
        if (auto temp = std::dynamic_pointer_cast<ir::Temp>(block.jump.arg)) {
            if (block.live_def.find(temp) ==
                block.live_def.end()) { // if not def, then use
                live_use.insert(temp);
            }
        }
    }
}

bool LivenessAnalysisPass::_update_live(ir::Block &block) {
    std::unordered_set<ir::TempPtr> old_in(block.live_in.begin(),
                                           block.live_in.end());
    // out[B] = ∪(s ∈ succ[B]) in[s]
    switch (block.jump.type) {
    case ir::Jump::JMP: {
        auto &succ_in = block.jump.blk[0]->live_in;
        block.live_out.insert(succ_in.begin(), succ_in.end());
    } break;
    case ir::Jump::JNZ: {
        auto &succ_in = block.jump.blk[0]->live_in;
        block.live_out.insert(succ_in.begin(), succ_in.end());
        auto &succ_in2 = block.jump.blk[1]->live_in;
        block.live_out.insert(succ_in2.begin(), succ_in2.end());
    } break;
    case ir::Jump::RET:
        break;
    default:
        throw std::runtime_error("invalid jump type");
    }

    // in[B] = in[B] ∪ (out[B] - def[B])
    std::unordered_set<ir::TempPtr> update;
    std::set_difference(block.live_out.begin(), block.live_out.end(),
                        block.live_def.begin(), block.live_def.end(),
                        std::inserter(update, update.begin()));
    block.live_in.insert(update.begin(), update.end());

    auto improved = old_in.size() != block.live_in.size();
    return improved;
}

bool FillIntervalPass::run_on_function(ir::Function &func) {
    int number = 0;
    for (auto block : func.rpo) {
        std::unordered_map<ir::TempPtr, int> first_def;
        std::unordered_map<ir::TempPtr, int> last_use;
        int first_number = number;
        _find_intervals_in_block(*block, first_def, last_use,
                                 block->temps_in_block, number);
        int last_number = number - 1;

        for (auto temp : block->temps_in_block) {
            auto is_local = block->live_in.find(temp) == block->live_in.end() &&
                            block->live_out.find(temp) == block->live_out.end();
            temp->is_local = is_local;

            temp->interval = {
                .start = std::numeric_limits<int>::max(),
                .end = -1,
            };

            if (block->live_in.find(temp) ==
                block->live_in.end()) { // if temp is not in the live_in set
                // live interval extended to the first def
                temp->interval.start =
                    std::min(temp->interval.start, first_def[temp]);
            } else {
                // first number of block is in the live interval
                temp->interval.start =
                    std::min(temp->interval.start, first_number);
            }
            if (block->live_out.find(temp) ==
                block->live_out.end()) { // if temp is not in the live_out set
                // live interval extended to the last use
                temp->interval.end =
                    std::max(temp->interval.end, last_use[temp]);
            } else {
                // last number of block is in the live interval
                temp->interval.end = std::max(temp->interval.end, last_number);
            }
        }
    }

    return false;
}

void FillIntervalPass::_find_intervals_in_block(
    ir::Block &block, std::unordered_map<ir::TempPtr, int> &first_def,
    std::unordered_map<ir::TempPtr, int> &last_use,
    std::unordered_set<ir::TempPtr> &temps_in_block, int &number) {
    // insts
    for (auto inst : block.insts) {
        for (int i = 0; i < 2; i++)
            if (auto temp = std::dynamic_pointer_cast<ir::Temp>(inst->arg[i]);
                temp) {
                temps_in_block.insert(temp);
                last_use[temp] = number;
            }

        if (inst->to) {
            auto temp = inst->to;
            temps_in_block.insert(temp);
            if (first_def.find(temp) == first_def.end()) {
                first_def[temp] = number;
            }
        }
        inst->number = number++;
    }
    // jump
    if (block.jump.type == ir::Jump::RET || block.jump.type == ir::Jump::JNZ) {
        if (auto temp = std::dynamic_pointer_cast<ir::Temp>(block.jump.arg);
            temp) {
            temps_in_block.insert(temp);
            last_use[temp] = number;
        }
    }
    block.jump.number = number++;
}

} // namespace opt
