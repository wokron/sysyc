#include "opt/pass/propa.h"

bool opt::LocalConstAndCopyPropagationPass::run_on_basic_block(ir::Block &block) {
    std::unordered_map<ir::ValuePtr, ir::ValuePtr> propagate_map;

    for (auto phi : block.phis) {
        if (phi->args.size() == 1) { // just like a copy
            propagate_map[phi->to] = phi->args[0].second;
        }
    }

    for (auto inst : block.insts) {
        // first propagate
        if (auto it = propagate_map.find(inst->arg[0]);
            it != propagate_map.end()) {
            inst->arg[0] = it->second;
        }
        if (auto it = propagate_map.find(inst->arg[1]);
            it != propagate_map.end()) {
            inst->arg[1] = it->second;
        }
        // then fold
        auto after_fold = _fold_if_can(*inst);
        if (after_fold != nullptr) {
            propagate_map.insert({inst->to, after_fold});
        }
    }

    if (block.jump.type == ir::Jump::JNZ) {
        if (auto constbits =
                std::dynamic_pointer_cast<ir::ConstBits>(block.jump.arg)) {
            // must be int
            auto constint = std::get_if<int>(&constbits->value);
            if (constint == nullptr) {
                throw std::logic_error("arg type of jnz must be int");
            }
            if (*constint) { // jmp true
                block.jump = {
                    .type = ir::Jump::JMP,
                    .arg = nullptr,
                    .blk = {block.jump.blk[0], nullptr},
                };
            } else { // jmp false
                block.jump = {
                    .type = ir::Jump::JMP,
                    .arg = nullptr,
                    .blk = {block.jump.blk[1], nullptr},
                };
            }
        } else if (block.jump.blk[0] == block.jump.blk[1]) {
            block.jump = {
                .type = ir::Jump::JMP,
                .arg = nullptr,
                .blk = {block.jump.blk[0], nullptr},
            };
        }
    }

    return false;
}

ir::ValuePtr
opt::LocalConstAndCopyPropagationPass::_fold_if_can(const ir::Inst &inst) {
    switch (inst.insttype) {
    case ir::InstType::ICOPY:
        return inst.arg[0];
    case ir::InstType::IADD:
        return _folder.fold_add(inst.arg[0], inst.arg[1]);
    case ir::InstType::ISUB:
        return _folder.fold_sub(inst.arg[0], inst.arg[1]);
    case ir::InstType::INEG:
        return _folder.fold_neg(inst.arg[0]);
    case ir::InstType::IDIV:
        return _folder.fold_div(inst.arg[0], inst.arg[1]);
    case ir::InstType::IREM:
        return _folder.fold_rem(inst.arg[0], inst.arg[1]);
    case ir::InstType::ICEQW:
    case ir::InstType::ICEQS:
        return _folder.fold_eq(inst.arg[0], inst.arg[1]);
    case ir::InstType::ICNEW:
    case ir::InstType::ICNES:
        return _folder.fold_ne(inst.arg[0], inst.arg[1]);
    case ir::InstType::ICSLEW:
    case ir::InstType::ICLES:
        return _folder.fold_le(inst.arg[0], inst.arg[1]);
    case ir::InstType::ICSLTW:
    case ir::InstType::ICLTS:
        return _folder.fold_lt(inst.arg[0], inst.arg[1]);
    case ir::InstType::ICSGEW:
    case ir::InstType::ICGES:
        return _folder.fold_ge(inst.arg[0], inst.arg[1]);
    case ir::InstType::ICSGTW:
    case ir::InstType::ICGTS:
        return _folder.fold_gt(inst.arg[0], inst.arg[1]);
    case ir::InstType::IEXTSW:
        return _folder.fold_extsw(inst.arg[0]);
    default:
        return nullptr;
    }
}

bool opt::CopyPropagationPass::run_on_function(ir::Function &func) {
    bool changed = false;
    std::unordered_map<ir::ValuePtr, ir::ValuePtr> copy_map;
    for (auto temp : func.temps_in_func) {
        if (temp->defs.size() != 1) {
            throw std::logic_error("ssa is required");
        }
        auto iter_temp = temp;
        do {
            auto instdef = std::get_if<ir::InstDef>(&iter_temp->defs[0]);
            if (instdef == nullptr) {
                break;
            }
            auto inst = instdef->ins;
            if (inst->insttype != ir::InstType::ICOPY) {
                break;
            }
            copy_map[temp] = inst->arg[0]; // insert will not replace the old
                                           // value, so we use []
            changed = true;
            iter_temp = std::dynamic_pointer_cast<ir::Temp>(inst->arg[0]);
        } while (iter_temp != nullptr);
    }

    for (auto temp : func.temps_in_func) {
        auto it = copy_map.find(temp);
        if (it == copy_map.end()) {
            continue;
        }

        auto target = it->second;

        for (auto use : temp->uses) {
            if (auto instuse = std::get_if<ir::InstUse>(&use)) {
                for (int i = 0; i < 2; i++)
                    if (temp == instuse->ins->arg[i]) {
                        instuse->ins->arg[i] = target;
                    }
            } else if (auto phiuse = std::get_if<ir::PhiUse>(&use)) {
                for (auto &[block, value] : phiuse->phi->args) {
                    if (temp == value) {
                        value = target;
                    }
                }
            } else if (auto jumpuse = std::get_if<ir::JmpUse>(&use)) {
                jumpuse->blk->jump.arg = target;
            } else {
                throw std::runtime_error("invalid use");
            }
        }
    }

    return changed;
}