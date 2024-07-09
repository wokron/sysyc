#include "opt/pass/ssa.h"

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
    // %temp =l allocn <bytes>
    // nop
    auto temp = alloc_inst->to;
    temp->defs.clear();
    temp->type = ir::Type::X;
    std::vector<ir::Use> uses(temp->uses.begin(), temp->uses.end());
    temp->uses.clear();

    alloc_inst->insttype = ir::InstType::INOP;
    alloc_inst->arg[0] = nullptr;
    alloc_inst->arg[1] = nullptr;
    alloc_inst->to = nullptr;

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
            // store %from %temp
            // %temp =t copy %from
            used_inst->insttype = ir::InstType::ICOPY;
            used_inst->to = temp;
            used_inst->arg[1] = nullptr;

            temp->type = used_inst->arg[0]->get_type();
            temp->defs.push_back(ir::InstDef{used_inst});
            break;
        case ir::InstType::ILOADS:
        case ir::InstType::ILOADL:
        case ir::InstType::ILOADW:
            // %to =t load %temp
            // %to =t copy %temp
            used_inst->insttype = ir::InstType::ICOPY;
            temp->type = used_inst->to->get_type();
            temp->uses.push_back(ir::InstUse{used_inst});
            break;
        default:
            throw std::logic_error("unable to mem2reg");
        }
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
            auto instdef = std::get_if<ir::InstDef>(&def);
            if (instdef == nullptr) {
                throw std::runtime_error("not inst def in when inserting phi");
            }
            temp_def_blocks.insert(instdef->blk);
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
                    for (auto pred : block->preds) {
                        phi_args.push_back({pred, temp});
                    }
                    auto phi = std::make_shared<ir::Phi>(
                        temp->get_type(), temp,
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
                    throw std::runtime_error("use must after def");
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

        std::vector<ir::BlockPtr> succs;
        switch (block->jump.type) {
        case ir::Jump::JNZ:
            succs.push_back(block->jump.blk[1]);
        case ir::Jump::JMP:
            succs.push_back(block->jump.blk[0]);
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
                        value = temp_rename_stack.empty()
                                    ? nullptr
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
}

ir::TempPtr opt::VariableRenamingPass::_create_temp_from(ir::TempPtr old_temp,
                                                         uint &temp_counter) {
    auto name = old_temp->name + "." + std::to_string(old_temp->id);
    auto new_temp = std::make_shared<ir::Temp>(name, old_temp->get_type(),
                                               std::vector<ir::Def>{});
    new_temp->id = temp_counter++;
    return new_temp;
}
