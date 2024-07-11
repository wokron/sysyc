#include "opt/pass/mem2reg.h"
#include <stack>

namespace opt {
std::stack<ir::ValuePtr> rename_stack;

bool Mem2regPass::run_on_function(ir::Function &func) {
    for (auto block = func.start; block; block = block->next) {
        for (auto it = block->insts.begin(); it != block->insts.end(); ++it) {
            auto inst = *it;
            auto value =
                std::dynamic_pointer_cast<ir::ConstBits>(inst->arg[0])->value;
            if ((inst->insttype == ir::InstType::IALLOC4 &&
                 std::get<int>(value) == 4) ||
                (inst->insttype == ir::InstType::IALLOC8 &&
                 std::get<int>(value) == 8)) {
                insert_phi(block, inst);
                rename_dfs(func.start, inst);
            }
        }
    }
}

void Mem2regPass::insert_phi(ir::BlockPtr block, ir::InstPtr instruction) {
    std::unordered_set<ir::BlockPtr> def_block_set;
    std::unordered_set<ir::BlockPtr> W;
    std::unordered_set<ir::BlockPtr> F;
    for (auto &def : instruction->to->defs) {
        if (std::holds_alternative<ir::PhiDef>(def)) {
            W.insert(std::get<ir::PhiDef>(def).blk);
            def_block_set.insert(std::get<ir::PhiDef>(def).blk);
        } else if (std::holds_alternative<ir::InstDef>(def)) {
            W.insert(std::get<ir::InstDef>(def).blk);
            def_block_set.insert(std::get<ir::InstDef>(def).blk);
        }
    }
    while (!W.empty()) {
        ir::BlockPtr X = *W.begin();
        W.erase(X);
        for (auto &Y : X->df_list) {
            if (F.find(Y) == F.end()) {
                ir::Type ty =
                    instruction->insttype == ir::InstType::IALLOC4 ? ir::Type::L : ir::Type::W;
                auto temp =
                    std::make_shared<ir::Temp>("", ty, std::vector<ir::Def>{});
                auto phi_instr = std::make_shared<ir::Phi>(ty, temp);
                Y->phis.push_back(phi_instr);
                instruction->to->defs.push_back(ir::PhiDef{phi_instr, block});
                instruction->to->uses.push_back(ir::PhiUse{phi_instr, block});
                F.insert(Y);
                if (def_block_set.find(Y) == def_block_set.end()) {
                    W.insert(Y);
                }
            }
        }
    }
}

void Mem2regPass::rename_dfs(ir::BlockPtr block, ir::InstPtr instruction) {
    int size = rename_stack.size();
    std::unordered_set<ir::PhiPtr> def_phi_set;
    std::unordered_set<ir::InstPtr> def_instr_set;
    for (auto &def : instruction->to->defs) {
        if (std::holds_alternative<ir::PhiDef>(def)) {
            def_phi_set.insert(std::get<ir::PhiDef>(def).phi);
        } else if (std::holds_alternative<ir::InstDef>(def)) {
            def_instr_set.insert(std::get<ir::InstDef>(def).ins);
        }
    }
    std::unordered_set<ir::PhiPtr> use_phi_set;
    std::unordered_set<ir::InstPtr> use_instr_set;
    for (auto &use : instruction->to->uses) {
        if (std::holds_alternative<ir::PhiUse>(use)) {
            use_phi_set.insert(std::get<ir::PhiUse>(use).phi);
        } else if (std::holds_alternative<ir::InstUse>(use)) {
            use_instr_set.insert(std::get<ir::InstUse>(use).ins);
        }
    }
    for (auto &phi : block->phis) {
        if (def_phi_set.find(phi) != def_phi_set.end()) {
            rename_stack.push(phi->to);
        }
    }
    ir::Type ty = ir::Type::W;
    for (auto it = block->insts.begin(); it != block->insts.end();) {
        auto inst = *it;
        if (inst->insttype == ir::InstType::IALLOC4 ||
            inst->insttype == ir::InstType::IALLOC8) {
            it = block->insts.erase(it);
        } else if ((inst->insttype == ir::InstType::ISTORES ||
                    inst->insttype == ir::InstType::ISTOREL ||
                    inst->insttype == ir::InstType::ISTOREW) &&
                   def_instr_set.find(inst) != def_instr_set.end()) {
            rename_stack.push(inst->arg[0]);
            it = block->insts.erase(it);
        } else if ((inst->insttype == ir::InstType::ILOADW ||
                    inst->insttype == ir::InstType::ILOADS ||
                    inst->insttype == ir::InstType::ILOADL) &&
                   use_instr_set.find(inst) != use_instr_set.end()) {
            ir::ValuePtr val;
            if (inst->insttype == ir::InstType::ILOADW) {
                ty = ir::Type::W;
                val = ir::ConstBits::get(0);
            } else if (inst->insttype == ir::InstType::ILOADS) {
                ty = ir::Type::S;
                val = ir::ConstBits::get(0.0f);
            } else if (inst->insttype == ir::InstType::ILOADL) {
                ty = ir::Type::L;
                val = ir::ConstBits::get(0);
            }
            if (!rename_stack.empty()) {
                val = rename_stack.top();
            }
            // modify all value
            for (auto &use : inst->to->uses) {
                if (std::holds_alternative<ir::PhiUse>(use)) {
                    std::get<ir::PhiUse>(use).phi->modify_args(inst->to, block,
                                                               val);
                } else if (std::holds_alternative<ir::InstUse>(use)) {
                    std::get<ir::InstUse>(use).ins->modify_args(inst->to, val);
                }
            }
            it = block->insts.erase(it);
        } else {
            it++;
        }
    }
    // modify type
    for (auto &phi : block->phis) {
        if (def_phi_set.find(phi) != def_phi_set.end() ||
            use_phi_set.find(phi) != use_phi_set.end()) {
            phi->to->type = ty;
        }
    }
    for (auto &suc_block : block->succs) {
        for (auto &phi : block->phis) {
            if (use_phi_set.find(phi) != use_phi_set.end()) {
                ir::ValuePtr val;
                if (phi->to->type == ir::Type::S) {
                    val = ir::ConstBits::get(0.0f);
                } else {
                    val = ir::ConstBits::get(0);
                }
                if (rename_stack.empty()) {
                    phi->args.emplace_back(block, val);
                } else {
                    phi->args.emplace_back(block, rename_stack.top());
                }
            }
        }
    }
    for (auto &dom_block : block->dom_children) {
        rename_dfs(dom_block, instruction);
    }
    while (rename_stack.size() > size) {
        rename_stack.pop();
    }
}

} // namespace opt