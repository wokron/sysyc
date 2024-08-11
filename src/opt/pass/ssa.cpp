#include "opt/pass/ssa.h"
#include <algorithm>

bool opt::MemoryToRegisterPass::run_on_function(ir::Function &func) {
    bool changed = false;
    for (auto inst : func.start->insts) {
        if (inst->insttype == ir::InstType::IALLOC4 ||
            inst->insttype == ir::InstType::IALLOC8) {
            changed |= _mem_to_reg(inst);
        }
    }
    return changed;
}

bool opt::MemoryToRegisterPass::_mem_to_reg(ir::InstPtr alloc_inst) {
    if (!_is_able_to_reg(alloc_inst)) {
        return false;
    }

    // just use to as new temp, and replace alloc
    // before: %temp =l allocn <bytes>
    // after: nop
    auto temp = alloc_inst->to;
    temp->type = ir::Type::W; // default type for var that never use or define
    std::vector<ir::Use> uses(temp->uses.begin(), temp->uses.end());
    temp->uses.clear();

    // alloc no longer needed, so replace it with nop
    *alloc_inst = {
        .insttype = ir::InstType::INOP,
        .to = nullptr,
        .arg = {nullptr, nullptr},
    };

    for (auto use : uses) {
        auto instuse = std::get_if<ir::InstUse>(&use);
        if (instuse == nullptr) {
            throw std::logic_error("unable to mem2reg");
        }
        auto used_inst = instuse->ins;
        switch (used_inst->insttype) {
        case ir::InstType::ISTORES:
        case ir::InstType::ISTOREL:
        case ir::InstType::ISTOREW:
            // before: store %from %temp
            // after: %temp =t copy %from
            used_inst->insttype = ir::InstType::ICOPY;
            used_inst->to = temp;
            used_inst->arg[1] = nullptr;

            temp->type = used_inst->arg[0]->get_type();
            temp->defs.push_back(ir::InstDef{used_inst, instuse->blk});
            break;
        case ir::InstType::ILOADS:
        case ir::InstType::ILOADL:
        case ir::InstType::ILOADW:
            // before: %to =t load %temp
            // after: %to =t copy %temp
            used_inst->insttype = ir::InstType::ICOPY;
            temp->type = used_inst->to->get_type();
            temp->uses.push_back(ir::InstUse{used_inst, instuse->blk});
            break;
        default:
            throw std::logic_error("unable to mem2reg");
        }
    }

    if (temp->type == ir::Type::W) {
        alloc_inst->arg[0] = ir::ConstBits::get(0);
    } else {
        alloc_inst->arg[0] = ir::ConstBits::get(0.0f);
    }

    return true;
}

bool opt::MemoryToRegisterPass::_is_able_to_reg(ir::InstPtr alloc_inst) {
    if (alloc_inst->to->defs.size() != 1) {
        return false;
    }
    for (auto use : alloc_inst->to->uses) {
        auto instuse = std::get_if<ir::InstUse>(&use);
        if (instuse == nullptr) {
            return false;
        }
        switch (instuse->ins->insttype) {
        case ir::InstType::ISTORES:
        case ir::InstType::ISTOREL:
        case ir::InstType::ISTOREW:
        case ir::InstType::ILOADS:
        case ir::InstType::ILOADL:
        case ir::InstType::ILOADW:
            continue;
        default:
            return false;
        }
    }

    return true;
}

bool opt::PhiInsertingPass::run_on_function(ir::Function &func) {
    for (auto temp : func.temps_in_func) {
        std::unordered_set<ir::BlockPtr> temp_def_blocks;
        for (auto def : temp->defs) {
            if (auto instdef = std::get_if<ir::InstDef>(&def)) {
                temp_def_blocks.insert(instdef->blk);
            } else if (auto phidef = std::get_if<ir::PhiDef>(&def)) {
                temp_def_blocks.insert(phidef->blk);
            } else {
                throw std::logic_error("invalide def type");
            }
        }

        if (temp_def_blocks.size() == 1) {
            continue;
        }

        std::unordered_set<ir::BlockPtr> phi_inserted_blocks;
        std::unordered_set<ir::BlockPtr> worklist(temp_def_blocks.begin(),
                                                  temp_def_blocks.end());

        while (!worklist.empty()) {
            auto it = worklist.begin();
            auto block = *it;
            worklist.erase(it);

            for (auto df : block->dfron) {
                if (phi_inserted_blocks.find(df) == phi_inserted_blocks.end()) {
                    // insert phi
                    decltype(ir::Phi::args) phi_args;
                    for (auto pred : df->preds) {
                        phi_args.push_back({pred, temp});
                    }
                    auto phi = std::make_shared<ir::Phi>(
                        temp,
                        phi_args); // %temp =t phi @b1 %temp, @b2 %temp, ...
                    df->phis.push_back(phi);
                    temp->defs.push_back(ir::PhiDef{phi, df});

                    worklist.insert(df);
                    if (temp_def_blocks.find(df) == temp_def_blocks.end()) {
                        temp_def_blocks.insert(df);
                        worklist.insert(df);
                    }
                }
            }
        }
    }

    return false;
}

bool opt::VariableRenamingPass::run_on_function(ir::Function &func) {
    RenameStack rename_stack;
    for (auto temp : func.temps_in_func) {
        rename_stack.insert({temp, std::stack<ir::TempPtr>()});
    }
    _dom_tree_preorder_traversal(func.start, rename_stack, func.temp_counter);

    return false;
}

void opt::VariableRenamingPass::_dom_tree_preorder_traversal(
    ir::BlockPtr block, RenameStack &rename_stack, uint &temp_counter) {
    std::vector<ir::TempPtr> renamed_temps;
    for (auto phi : block->phis) {
        auto new_temp = _create_temp_from(phi->to, temp_counter);
        new_temp->defs.push_back(ir::PhiDef{phi, block});

        rename_stack.at(phi->to).push(new_temp);
        renamed_temps.push_back(phi->to);
        phi->to = new_temp;
    }

    for (auto inst : block->insts) {
        // uses
        for (int i = 0; i < 2; i++)
            if (auto temp = std::dynamic_pointer_cast<ir::Temp>(inst->arg[i])) {
                if (rename_stack.at(temp).empty()) {
                    // throw std::runtime_error("use before def");
                    continue; // TODO: maybe wrong
                }
                inst->arg[i] = rename_stack.at(temp).top();
            }
        // def
        if (inst->to != nullptr) {
            if (inst->to->defs.size() == 1) { // don't need to rename
                rename_stack.at(inst->to).push(inst->to);
                renamed_temps.push_back(inst->to);
            } else {
                auto new_temp = _create_temp_from(inst->to, temp_counter);
                new_temp->defs.push_back(ir::InstDef{inst, block});

                rename_stack.at(inst->to).push(new_temp);
                renamed_temps.push_back(inst->to);
                inst->to = new_temp;
            }
        }
    }

    if (auto temp = std::dynamic_pointer_cast<ir::Temp>(block->jump.arg)) {
        if (rename_stack.at(temp).empty()) {
            throw std::runtime_error("use before def");
        }
        block->jump.arg = rename_stack.at(temp).top();
    }

    std::unordered_set<ir::BlockPtr> succs;
    switch (block->jump.type) {
    case ir::Jump::JNZ:
        succs.insert(block->jump.blk[1]);
    case ir::Jump::JMP:
        succs.insert(block->jump.blk[0]);
        break;
    default:
        break;
    }

    for (auto succ : succs) {
        for (auto phi : succ->phis) {
            for (auto &[src_block, value] : phi->args) {
                auto temp = std::dynamic_pointer_cast<ir::Temp>(value);
                if (temp && src_block == block) {
                    auto &temp_rename_stack = rename_stack.at(temp);
                    value = temp_rename_stack.empty() ? nullptr
                                                      : temp_rename_stack.top();
                }
            }
        }
    }

    for (auto child : block->doms) {
        _dom_tree_preorder_traversal(child, rename_stack, temp_counter);
    }

    for (auto temp : renamed_temps) {
        rename_stack.at(temp).pop();
    }
}

ir::TempPtr opt::VariableRenamingPass::_create_temp_from(ir::TempPtr old_temp,
                                                         uint &temp_counter) {
    auto name = old_temp->name + "." + std::to_string(old_temp->id);
    auto new_temp = std::make_shared<ir::Temp>(name, old_temp->get_type(),
                                               std::vector<ir::Def>{});
    new_temp->id = temp_counter++;
    return new_temp;
}

bool opt::SSADestructPass::run_on_function(ir::Function &func) {
    std::vector<ir::BlockPtr> blocks; // since this pass could change block
                                      // structure, we save all blocks first
    for (auto block = func.start; block; block = block->next) {
        blocks.push_back(block);
    }

    std::unordered_map<ir::BlockPtr, ParallelCopy> parallel_copy_map;

    _split_critical_edge(blocks, parallel_copy_map, func.block_counter_ptr);
    _parallel_copy_to_sequential(parallel_copy_map, func.temp_counter);

    return false;
}

void opt::SSADestructPass::_split_critical_edge(
    const std::vector<ir::BlockPtr> &blocks, ParallelCopyMap &parallel_copy_map,
    uint *block_counter_ptr) {
    for (auto block : blocks) {
        if (block->phis.size() == 0) { // no phis to destruct
            continue;
        }
        for (auto pred : block->preds) {
            // create parallel copy
            if (pred->jump.type ==
                ir::Jump::JNZ) { // pred has several outgoing edges
                // create new block after pred
                auto new_block = std::shared_ptr<ir::Block>(
                    new ir::Block{(*block_counter_ptr)++, "parallel_copy"});
                new_block->next = pred->next;
                pred->next = new_block;
                new_block->jump = {
                    .type = ir::Jump::JMP,
                    .blk = {block},
                };
                if (pred->jump.blk[0] == block) {
                    pred->jump.blk[0] = new_block;
                }
                if (pred->jump.blk[1] == block) {
                    pred->jump.blk[1] = new_block;
                }
                parallel_copy_map.insert({new_block, ParallelCopy()});
                _update_phis(block->phis, pred, new_block);
            } else {
                parallel_copy_map.insert({pred, ParallelCopy()});
            }
        }

        for (auto phi : block->phis) {
            for (auto [block, value] : phi->args) {
                parallel_copy_map.at(block).push_back({phi->to, value});
            }
        }
        block->phis.clear();
    }
}

void opt::SSADestructPass::_parallel_copy_to_sequential(
    ParallelCopyMap &parallel_copy_map, uint &temp_counter) {
    auto n_l =
        std::make_shared<ir::Temp>("n", ir::Type::L, std::vector<ir::Def>{});
    n_l->id = temp_counter++;
    auto n_s =
        std::make_shared<ir::Temp>("n", ir::Type::S, std::vector<ir::Def>{});
    n_s->id = temp_counter++;
    auto n_w =
        std::make_shared<ir::Temp>("n", ir::Type::W, std::vector<ir::Def>{});
    n_w->id = temp_counter++;

    for (auto &[block, pc] : parallel_copy_map) {
        pc.erase(std::remove_if(
                     pc.begin(), pc.end(),
                     [](const std::pair<ir::ValuePtr, ir::ValuePtr> &elm) {
                         return elm.first == elm.second;
                     }),
                 pc.end());

        std::vector<ir::ValuePtr> ready;
        std::vector<ir::ValuePtr> todo;
        std::unordered_map<ir::ValuePtr, ir::ValuePtr> loc;
        std::unordered_map<ir::ValuePtr, ir::ValuePtr> directpred;
        directpred[n_l] = nullptr;
        directpred[n_s] = nullptr;
        directpred[n_w] = nullptr;

        for (auto [b, a] : pc) {
            loc[b] = nullptr;
            directpred[a] = nullptr;
        }
        for (auto [b, a] : pc) {
            loc[a] = a;
            directpred[b] = a;
            todo.push_back(b);
        }
        for (auto [b, a] : pc) {
            if (loc[b] == nullptr) {
                ready.push_back(b);
            }
        }
        while (!todo.empty()) {
            while (!ready.empty()) {
                auto b = ready.back();
                ready.pop_back();
                auto a = directpred[b];
                auto c = loc[a];
                _emit_copy(block, b, c);
                loc[a] = b;
                if (a == c && directpred[a] != nullptr) {
                    ready.push_back(a);
                }
            }
            auto b = todo.back();
            todo.pop_back();
            if (b == loc[directpred[b]]) {
                ir::ValuePtr n;
                switch (b->get_type()) {
                case ir::Type::L:
                    n = n_l;
                    break;
                case ir::Type::S:
                    n = n_s;
                    break;
                case ir::Type::W:
                    n = n_w;
                    break;
                default:
                    throw std::logic_error("unsupported type");
                }
                _emit_copy(block, n, b);
                loc[b] = n;
                ready.push_back(b);
            }
        }
    }
}

void opt::SSADestructPass::_emit_copy(ir::BlockPtr block, ir::ValuePtr to,
                                      ir::ValuePtr from) {
    if (from == nullptr || from == to) { // no need to copy
        return;
    }
    auto to_temp = std::dynamic_pointer_cast<ir::Temp>(to);
    if (to_temp == nullptr) {
        throw std::runtime_error("copy target is not tmep");
    }
    block->insts.push_back(std::shared_ptr<ir::Inst>(new ir::Inst{
        ir::InstType::ICOPY,
        to_temp,
        {from, nullptr},
    }));
}

void opt::SSADestructPass::_update_phis(std::vector<ir::PhiPtr> &phis,
                                        ir::BlockPtr old_block,
                                        ir::BlockPtr new_block) {
    for (auto &phi : phis) {
        for (auto &[block, value] : phi->args) {
            if (block == old_block) {
                block = new_block;
            }
        }
    }
}

bool opt::SimpleRemoveCopyAfterSSADestructPass::run_on_function(
    ir::Function &func) {
    for (auto block = func.start; block; block = block->next) {
        for (auto inst : block->insts) {
            if (inst->insttype == ir::InstType::ICOPY) {
                if (auto temp =
                        std::dynamic_pointer_cast<ir::Temp>(inst->arg[0]);
                    temp && temp->defs.size() == 1 && temp->uses.size() == 1) {
                    auto instdef = std::get<ir::InstDef>(temp->defs[0]);
                    if (instdef.blk != block ||
                        instdef.ins->insttype == ir::InstType::IPAR) {
                        continue;
                    }
                    instdef.ins->to = inst->to;
                    *inst = {
                        .insttype = ir::InstType::INOP,
                        .to = nullptr,
                        .arg = {nullptr, nullptr},
                    };
                }
            }
        }
    }

    return true;
}