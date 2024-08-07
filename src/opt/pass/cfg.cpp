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
    func.temps_in_func.clear();

    for (auto block = func.start; block; block = block->next) {
        block->temps_in_block.clear();

        for (auto &phi : block->phis) {
            phi->to->defs.clear();
            phi->to->uses.clear();

            block->temps_in_block.insert(phi->to);
            func.temps_in_func.insert(phi->to);
        }
        for (auto &inst : block->insts) {
            if (inst->to) {
                inst->to->defs.clear();
                inst->to->uses.clear();

                block->temps_in_block.insert(inst->to);
                func.temps_in_func.insert(inst->to);
            }
        }
    }

    for (auto block = func.start; block; block = block->next) {
        // phi
        for (auto &phi : block->phis) {
            phi->to->defs.push_back(ir::PhiDef{phi, block});
            for (auto [blk, arg] : phi->args) {
                if (auto temp = std::dynamic_pointer_cast<ir::Temp>(arg);
                    temp) {
                    temp->uses.push_back(ir::PhiUse{phi, block});
                }
            }
        }
        // inst
        for (auto &inst : block->insts) {
            if (inst->to) {
                inst->to->defs.push_back(ir::InstDef{inst, block});
            }
            if (auto temp = std::dynamic_pointer_cast<ir::Temp>(inst->arg[0]);
                temp) {
                temp->uses.push_back(ir::InstUse{inst, block});
            }
            if (auto temp = std::dynamic_pointer_cast<ir::Temp>(inst->arg[1]);
                temp) {
                temp->uses.push_back(ir::InstUse{inst, block});
            }
        }
        // jump
        if (auto temp = std::dynamic_pointer_cast<ir::Temp>(block->jump.arg);
            temp) {
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
        auto elm = *it;
        elm->rpo_id = func.rpo.size();
        func.rpo.push_back(elm);
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
        _post_order_traverse(block->jump.blk[1], visited, post_order);
        _post_order_traverse(block->jump.blk[0], visited, post_order);
        break;
    case ir::Jump::RET:
        break;
    default:
        throw std::runtime_error("unknown jump type");
    }
    post_order.push_back(block);
}

bool opt::CooperFillDominatorsPass::run_on_function(ir::Function &func) {
    for (auto block = func.start; block; block = block->next) {
        block->idom = nullptr;
        block->doms.clear();
    }

    bool improved = false;
    do {
        improved = false;
        for (auto block : func.rpo) {
            if (block == func.start) {
                continue;
            }
            ir::BlockPtr new_idom = nullptr;
            for (auto pred : block->preds) {
                if (pred->idom || pred == func.start) {
                    new_idom = _intersect(new_idom, pred);
                }
            }
            if (new_idom != block->idom) {
                improved = true;
                block->idom = new_idom;
            }
        }
    } while (improved);

    // reversed relation
    for (auto block = func.start; block; block = block->next) {
        if (block->idom != nullptr) {
            block->idom->doms.push_back(block);
        }
    }

    return false;
}

ir::BlockPtr CooperFillDominatorsPass::_intersect(ir::BlockPtr b1,
                                                  ir::BlockPtr b2) {
    if (b1 == nullptr) {
        return b2;
    }
    while (b1 != b2) {
        if (b1->rpo_id < b2->rpo_id) {
            b1.swap(b2);
        }
        while (b1->rpo_id > b2->rpo_id) {
            b1 = b1->idom;
            if (b1 == nullptr) {
                throw std::runtime_error("cooper's fill dom algorithm fail");
            }
        }
    }
    return b1;
}

bool FillDominanceFrontierPass::run_on_function(ir::Function &func) {
    for (auto block = func.start; block; block = block->next) {
        block->dfron.clear();
    }
    for (auto block = func.start; block; block = block->next) {
        ir::BlockPtr a, b, x; // edge a -> b
        a = block;
        switch (block->jump.type) {
        case ir::Jump::JNZ:
            b = block->jump.blk[1];
            x = a;
            while (!_is_strictly_dominate(x, b)) {
                x->dfron.push_back(b);
                x = x->idom;
            }
            // fallthrough
        case ir::Jump::JMP:
            b = block->jump.blk[0];
            x = a;
            while (!_is_strictly_dominate(x, b)) {
                x->dfron.push_back(b);
                x = x->idom;
            }
            break;
        default:
            break;
        }
    }

    return false;
}

bool FillDominanceFrontierPass::_is_strictly_dominate(ir::BlockPtr b1,
                                                      ir::BlockPtr b2) {
    if (b1 == nullptr || b2 == nullptr) {
        throw std::runtime_error("fail to fill dominate frontier");
    }
    if (b1 == b2) {
        return true;
    }
    while (b2->rpo_id > b1->rpo_id) {
        b2 = b2->idom;
    }
    return b1 == b2;
}

} // namespace opt
