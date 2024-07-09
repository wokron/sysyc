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
