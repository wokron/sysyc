#include "opt/pass/cfg.h"

namespace opt {

bool FillPredsPass::run_on_function(ir::Function &func) {
    for (auto block = func.start; block; block = block->next) {
        block->preds.clear();
    }

    for (auto block = func.start; block; block = block->next) {
        switch (block->jump.type) {
        case ir::Jump::JMP:
            block->jump.blk[0]->preds.push_back(block);
            break;
        case ir::Jump::JNZ:
            block->jump.blk[0]->preds.push_back(block);
            if (block->jump.blk[1] != block->jump.blk[0]) {
                block->jump.blk[1]->preds.push_back(block);
            }
            break;
        case ir::Jump::RET:
            break;
        default:
            throw std::runtime_error("unknown jump type");
        }
    }

    return false;
}

bool FillUsesPass::run_on_function(ir::Function &func) {
    for (auto block = func.start; block; block = block->next) {
        for (auto &phi : block->phis) {
            phi->to->uses.clear();
        }
        for (auto &inst : block->insts) {
            if (inst->to) {
                inst->to->uses.clear();
            }
        }
    }

    for (auto block = func.start; block; block = block->next) {
        // phi
        for (auto &phi : block->phis) {
            phi->to->def = ir::PhiDef{phi};
            for (auto [blk, arg] : phi->args) {
                if (auto temp = std::dynamic_pointer_cast<ir::Temp>(arg)) {
                    temp->uses.push_back(ir::PhiUse{phi, block});
                }
            }
        }
        // inst
        for (auto &inst : block->insts) {
            if (inst->to) {
                inst->to->def = ir::InstDef{inst};
            }
            if (auto temp = std::dynamic_pointer_cast<ir::Temp>(inst->arg[0])) {
                temp->uses.push_back(ir::InstUse{inst});
            }
            if (auto temp = std::dynamic_pointer_cast<ir::Temp>(inst->arg[1])) {
                temp->uses.push_back(ir::InstUse{inst});
            }
        }
        // jump
        if (auto temp = std::dynamic_pointer_cast<ir::Temp>(block->jump.arg)) {
            temp->uses.push_back(ir::JmpUse{block});
        }
    }

    return false;
}

} // namespace opt
