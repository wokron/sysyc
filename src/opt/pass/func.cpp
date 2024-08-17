#include "opt/pass/func.h"
#include <algorithm>

namespace opt {

bool FillLeafPass::run_on_module(ir::Module &module) {
    for (auto &func : module.functions) {
        func->is_leaf = _is_leaf_function(*func);
    }

    return false;
}
bool FillLeafPass::_is_leaf_function(ir::Function &func) {
    for (auto block = func.start; block; block = block->next) {
        for (auto inst : block->insts) {
            if (inst->insttype == ir::InstType::ICALL) {
                return false;
            }
        }
    }
    return true;
}

} // namespace opt

bool opt::FillInlinePass::run_on_function(ir::Function &func) {
    func.is_inline = _is_inline(func);

    return false;
}

bool opt::FillInlinePass::_is_inline(const ir::Function &func) {
    int block_count = 0;
    for (auto block = func.start; block; block = block->next) {
        block_count++;

        for (auto inst : block->insts) {
            if (inst->insttype == ir::InstType::ICALL) {
                auto addr = std::static_pointer_cast<ir::Address>(inst->arg[0]);
                if (addr->ref_func == &func) { // direct recursive
                    // since sysy cannot separate func's decl and def, indirect
                    // recursive is impossible
                    return false;
                }
            }
        }
    }

    return true; // very aggressive
}

bool opt::FunctionInliningPass::run_on_function(ir::Function &func) {
    for (auto block = func.start; block; block = block->next) {
        std::vector<ir::ValuePtr> args;
        ir::TempPtr ret = nullptr;
        for (auto it = block->insts.begin(); it != block->insts.end(); it++) {
            auto inst = *it;
            if (inst->insttype == ir::InstType::IARG) {
                args.push_back(inst->arg[0]);
            } else if (inst->insttype == ir::InstType::ICALL) {
                ret = inst->to;
                auto addr =
                    std::dynamic_pointer_cast<ir::Address>(inst->arg[0]);
                if (addr->ref_func != nullptr && addr->ref_func->is_inline) {
                    // do inline
                    uint *block_counter_ptr = func.block_counter_ptr;
                    auto new_block = std::shared_ptr<ir::Block>(
                        new ir::Block{(*block_counter_ptr)++, "inline_join"});
                    // insert new block
                    new_block->next = block->next;
                    block->next = new_block;
                    // move insts
                    new_block->insts.assign(it + 1, block->insts.end());
                    block->insts.erase(it - args.size(), block->insts.end());
                    // move jump
                    new_block->jump = block->jump;
                    block->jump = {
                        .type = ir::Jump::JMP,
                        .blk = {new_block, nullptr},
                    };

                    _do_inline(block, *addr->ref_func, func, args, ret);

                    block->jump.blk[0] = block->next;
                    break;
                }
                args.clear();
            }
        }
    }
    return false;
}

void opt::FunctionInliningPass::_do_inline(
    ir::BlockPtr prev, ir::Function &inline_func, ir::Function &target_func,
    const std::vector<ir::ValuePtr> &args, ir::TempPtr ret_target) {
    // attention, counter is target_func's
    uint *block_counter_ptr = target_func.block_counter_ptr;
    uint &temp_counter = target_func.temp_counter;
    const auto after = prev->next;

    std::unordered_map<ir::BlockPtr, ir::BlockPtr> block_map;
    std::unordered_map<ir::ValuePtr, ir::ValuePtr> value_map;
    std::vector<ir::BlockPtr> new_blocks;

    // copy blocks
    auto p = prev;
    int arg_index = 0;
    for (auto block = inline_func.start; block;
         p = p->next, block = block->next) {
        auto new_name = block->get_name();
        auto new_block = std::shared_ptr<ir::Block>(
            new ir::Block{(*block_counter_ptr)++, new_name});
        // insert block
        new_block->next = p->next;
        p->next = new_block;

        block_map.insert({block, new_block});
        new_blocks.push_back(new_block);

        // for each new block, copy insts
        // first, phis
        for (auto phi : block->phis) {
            auto new_phi = std::shared_ptr<ir::Phi>(new ir::Phi(*phi));
            new_block->phis.push_back(new_phi);
            auto name = phi->to->name + "." + std::to_string(phi->to->id);
            auto new_to = std::make_shared<ir::Temp>(name, phi->to->get_type(),
                                                     std::vector<ir::Def>{});
            new_to->id = temp_counter++;
            new_phi->to = new_to;
            value_map.insert({phi->to, new_phi->to});
        }
        // then, insts
        for (auto inst : block->insts) {
            auto new_inst = std::shared_ptr<ir::Inst>(new ir::Inst(*inst));
            if (new_inst->insttype == ir::InstType::IALLOC4 ||
                new_inst->insttype == ir::InstType::IALLOC8) {
                target_func.start->insts.push_back(new_inst);
            } else {
                new_block->insts.push_back(new_inst);
            }

            if (new_inst->insttype == ir::InstType::IPAR) { // convert to copy
                auto src_arg = args[arg_index++];
                // before: %par =t par
                // after: %par =t copy %arg
                new_inst->insttype = ir::InstType::ICOPY;
                new_inst->arg[0] = src_arg;
            }

            if (inst->to != nullptr) {
                if (auto it = value_map.find(inst->to); it != value_map.end()) {
                    new_inst->to =
                        std::static_pointer_cast<ir::Temp>(it->second);
                } else {
                    auto name =
                        inst->to->name + "." + std::to_string(inst->to->id);
                    auto new_to = std::make_shared<ir::Temp>(
                        name, inst->to->get_type(), std::vector<ir::Def>{});
                    new_to->id = temp_counter++;
                    new_inst->to = new_to;
                    value_map.insert({inst->to, new_inst->to});
                }
            }
        }
        // list, just copy jump
        new_block->jump = block->jump;
    }

    // replace blocks and values
    for (auto new_block : new_blocks) {
        for (auto phi : new_block->phis) {
            for (auto &[block, value] : phi->args) {
                if (auto it = value_map.find(value); it != value_map.end()) {
                    value = it->second;
                }
                block = block_map.at(block);
            }
        }

        for (auto inst : new_block->insts) {
            for (int i = 0; i < 2; i++)
                if (inst->arg[i] != nullptr) {
                    if (auto it = value_map.find(inst->arg[i]);
                        it != value_map.end()) {
                        inst->arg[i] = it->second;
                    }
                }
        }

        // replace target blocks in jump
        switch (new_block->jump.type) {
        case ir::Jump::JNZ:
            if (auto it = value_map.find(new_block->jump.arg);
                it != value_map.end()) {
                new_block->jump.arg = it->second;
            }
            new_block->jump.blk[1] = block_map.at(new_block->jump.blk[1]);
        case ir::Jump::JMP:
            new_block->jump.blk[0] = block_map.at(new_block->jump.blk[0]);
            break;
        case ir::Jump::RET: // convert ret to jmp, the target is the first block
                            // after inlining blocks
            if (auto it = value_map.find(new_block->jump.arg);
                it != value_map.end()) {
                new_block->jump.arg = it->second;
            }
            if (new_block->jump.arg != nullptr) { // copy to ret_target
                auto new_inst = std::shared_ptr<ir::Inst>(new ir::Inst{
                    .insttype = ir::InstType::ICOPY,
                    .to = ret_target,
                    .arg = {new_block->jump.arg},
                });
                new_block->insts.push_back(new_inst);
            }
            new_block->jump = {
                .type = ir::Jump::JMP,
                .blk = {after, nullptr},
            };
            break;
        default:
            throw std::logic_error("unexpected jump type");
        }
    }
}

bool opt::TailRecursionElimination::run_on_function(ir::Function &func) {
    _create_jump_target_block(func);
    auto target = func.start->next;

    std::vector<ir::TempPtr> args;
    for (auto inst : func.start->insts) {
        if (inst->insttype == ir::InstType::IPAR) {
            args.push_back(inst->to);
        }
    }

    for (auto block = func.start; block; block = block->next) {
        if (_is_tail_recursive(func, *block)) {
            // replace args
            // before: arg %arg
            // after: %par =t copy %arg
            for (int i = 0; i < args.size(); i++) {
                auto dst_idx = args.size() - i - 1;
                auto src_idx = block->insts.size() - i - 2;
                auto inst = block->insts[src_idx];
                auto arg = args[dst_idx];
                *inst = {
                    .insttype = ir::InstType::ICOPY,
                    .to = arg,
                    .arg = {inst->arg[0]},
                };
            }

            // don't forget to remove the last call
            block->insts.pop_back();

            // replace jump
            block->jump = {
                .type = ir::Jump::JMP,
                .blk = {target, nullptr},
            };
        }
    }

    return false;
}

void opt::TailRecursionElimination::_create_jump_target_block(
    ir::Function &func) {
    // create a new block after start
    uint *block_counter_ptr = func.block_counter_ptr;
    auto new_block = std::shared_ptr<ir::Block>(
        new ir::Block{(*block_counter_ptr)++, "tail_recursion_target"});
    new_block->next = func.start->next;
    func.start->next = new_block;
    // manage jump
    new_block->jump = func.start->jump;
    func.start->jump = {
        .type = ir::Jump::JMP,
        .blk = {new_block, nullptr},
    };

    // move insts in start to new block, except par and alloc
    for (auto inst : func.start->insts) {
        if (inst->insttype == ir::InstType::IPAR ||
            inst->insttype == ir::InstType::IALLOC4 ||
            inst->insttype == ir::InstType::IALLOC8) {
            continue;
        }
        new_block->insts.push_back(inst);
    }
    func.start->insts.erase(
        std::remove_if(func.start->insts.begin(), func.start->insts.end(),
                       [](const ir::InstPtr &inst) {
                           return inst->insttype != ir::InstType::IPAR &&
                                  inst->insttype != ir::InstType::IALLOC4 &&
                                  inst->insttype != ir::InstType::IALLOC8;
                       }),
        func.start->insts.end());
}

bool opt::TailRecursionElimination::_is_tail_recursive(const ir::Function &func,
                                                       const ir::Block &block) {
    auto func_name = func.name;
    if (block.insts.empty() ||
        block.insts.back()->insttype != ir::InstType::ICALL ||
        block.jump.type != ir::Jump::RET) {
        return false;
    }

    auto inst = block.insts.back();
    auto addr = std::static_pointer_cast<ir::Address>(inst->arg[0]);
    return addr->name == func_name && func.start->next;
}