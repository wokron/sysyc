#include "opt/pass/live.h"
#include <algorithm>

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
                if (block.live_def.find(temp) == block.live_def.end()) { // if not def, then use
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
            if (block.live_def.find(temp) == block.live_def.end()) { // if not def, then use
                live_use.insert(temp);
            }
        }
        if (auto temp = std::dynamic_pointer_cast<ir::Temp>(inst->arg[1])) { // if not def, then use
            if (block.live_def.find(temp) == block.live_def.end()) {
                live_use.insert(temp);
            }
        }
        if (inst->to) {
            if (live_use.find(inst->to) == live_use.end()) { // if not use, then def
                block.live_def.insert(inst->to);
            }
        }
    }

    // jump
    if (block.jump.type == ir::Jump::JNZ || block.jump.type == ir::Jump::RET) {
        if (auto temp = std::dynamic_pointer_cast<ir::Temp>(block.jump.arg)) {
            if (block.live_def.find(temp) == block.live_def.end()) { // if not def, then use
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

} // namespace opt
