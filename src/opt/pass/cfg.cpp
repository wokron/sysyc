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
            phi->to->defs.clear();
            phi->to->uses.clear();
        }
        for (auto &inst : block->insts) {
            if (inst->to) {
                inst->to->defs.clear();
                inst->to->uses.clear();
            }
        }
    }

    for (auto block = func.start; block; block = block->next) {
        // phi
        for (auto &phi : block->phis) {
            phi->to->defs.push_back(ir::PhiDef{phi});
            for (auto [blk, arg] : phi->args) {
                if (auto temp = std::dynamic_pointer_cast<ir::Temp>(arg); temp) {
                    temp->uses.push_back(ir::PhiUse{phi, block});
                }
            }
        }
        // inst
        for (auto &inst : block->insts) {
            if (inst->to) {
                inst->to->defs.push_back(ir::InstDef{inst});
            }
            if (auto temp = std::dynamic_pointer_cast<ir::Temp>(inst->arg[0]); temp) {
                temp->uses.push_back(ir::InstUse{inst});
            }
            if (auto temp = std::dynamic_pointer_cast<ir::Temp>(inst->arg[1]); temp) {
                temp->uses.push_back(ir::InstUse{inst});
            }
        }
        // jump
        if (auto temp = std::dynamic_pointer_cast<ir::Temp>(block->jump.arg); temp) {
            temp->uses.push_back(ir::JmpUse{block});
        }
    }

    return false;
}

bool FillReversePostOrderPass::run_on_function(ir::Function &func) {
    func.rpo.clear();

    std::unordered_set<ir::BlockPtr> visited;
    std::vector<ir::BlockPtr> post_order;

    // dfs
    for (auto block = func.start; block; block = block->next) {
        if (visited.find(block) == visited.end()) {
            _post_order_traverse(block, visited, post_order);
        }
    }

    // reverse
    for (auto it = post_order.rbegin(); it != post_order.rend(); ++it) {
        func.rpo.push_back(*it);
    }

    return false;
}

void FillReversePostOrderPass::_post_order_traverse(
    ir::BlockPtr block, std::unordered_set<ir::BlockPtr> &visited,
    std::vector<ir::BlockPtr> &post_order) {
    if (!visited.insert(block).second) {
        return;
    }

    switch (block->jump.type) {
    case ir::Jump::JMP:
        _post_order_traverse(block->jump.blk[0], visited, post_order);
        break;
    case ir::Jump::JNZ:
        _post_order_traverse(block->jump.blk[0], visited, post_order);
        _post_order_traverse(block->jump.blk[1], visited, post_order);
        break;
    case ir::Jump::RET:
        break;
    default:
        throw std::runtime_error("unknown jump type");
    }
    post_order.push_back(block);
}

} // namespace opt
