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
                    df->phis.push_back(std::make_shared<ir::Phi>(
                        temp->get_type(), temp)); // %temp =t phi ...
                    worklist.insert(df);
                    if (temp_def_blocks.find(df) == temp_def_blocks.end()) {
                        worklist.insert(df);
                    }
                }
            }
        }
    }

    return false;
}
