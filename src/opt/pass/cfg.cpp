#include "opt/pass/cfg.h"

namespace opt {

std::unordered_set<ir::BlockPtr> temp_list;

bool FillPredsPass::run_on_function(ir::Function &func) {
    for (auto block = func.start; block; block = block->next) {
        block->preds.clear();
        block->succs.clear();
    }

    for (auto block = func.start; block; block = block->next) {
        switch (block->jump.type) {
        case ir::Jump::JMP:
            block->jump.blk[0]->preds.push_back(block);
            block->succs.push_back(block->jump.blk[0]);
            break;
        case ir::Jump::JNZ:
            block->jump.blk[0]->preds.push_back(block);
            block->succs.push_back(block->jump.blk[0]);
            if (block->jump.blk[1] != block->jump.blk[0]) {
                block->jump.blk[1]->preds.push_back(block);
                block->succs.push_back(block->jump.blk[1]);
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
            phi->to->defs.push_back(ir::PhiDef{phi,block});
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
                inst->to->defs.push_back(ir::InstDef{inst,block});
            }
            if (auto temp = std::dynamic_pointer_cast<ir::Temp>(inst->arg[0]);
                temp) {
                temp->uses.push_back(ir::InstUse{inst});
            }
            if (auto temp = std::dynamic_pointer_cast<ir::Temp>(inst->arg[1]);
                temp) {
                temp->uses.push_back(ir::InstUse{inst});
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

void DomListPass::dfs_exclude(ir::BlockPtr exclude_block,
                 ir::BlockPtr cur_block) {
    if (exclude_block == cur_block) {
        return;
    }
    temp_list.insert(cur_block);
    for (auto &block : cur_block->succs) {
        if (temp_list.find(block) != temp_list.end()) {
            dfs_exclude(exclude_block, block);
        }
    }
}

bool DomListPass::run_on_function(ir::Function &func) {
    for (auto block = func.start; block; block = block->next) {
        block->dom_list.clear();
    }

    for (auto block = func.start; block; block = block->next) {
        temp_list.clear();
        dfs_exclude(block, func.start);
        for (auto temp_block = func.start; temp_block;
             temp_block = temp_block->next) {
            if (temp_list.find(temp_block) == temp_list.end() &&
                block->dom_list.find(temp_block) == block->dom_list.end()) {
                block->dom_list.insert(temp_block);
            }
        }
    }

    return false;
}



bool DomTreePass::run_on_function(ir::Function &func) {
    for (auto block = func.start; block; block = block->next) {
        block->dom_children.clear();
    }
    for (auto block = func.start; block; block = block->next) {
        for (auto &dom_block : block->dom_list) {
            if (block != dom_block) {
                bool flag = true;
                for (auto &other_block : block->dom_list) {
                    if (other_block != block && other_block != dom_block) {
                        if (other_block->dom_list.find(dom_block) !=
                            other_block->dom_list.end()) {
                            flag = false;
                            break;
                        }
                    }
                }
                if (flag) {
                    block->dom_children.insert(dom_block);
                    block->dom_parent = block;
                }
            }
        }
    }
    return false;
}

bool DFPass::run_on_function(ir::Function &func) {
    for (auto block = func.start; block; block = block->next) {
        block->df_list.clear();
    }
    for (auto u_block = func.start; u_block; u_block = u_block->next) {
        for (auto &v_block : u_block->succs) {
            auto x_block = u_block;
            while (x_block->dom_list.find(v_block) == x_block->dom_list.end() ||
                   x_block == v_block) {
                x_block->df_list.insert(v_block);
                x_block = x_block->dom_parent;
            }
        }
    }
    return false;
}

} // namespace opt
