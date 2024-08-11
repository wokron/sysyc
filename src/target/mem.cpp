#include "target/mem.h"
#include "target/regalloc.h"
#include <set>

namespace target {

#define ROUND(a, n) (((((int)(a)) + (n) - 1)) & ~((n) - 1))

void StackManager::run(const ir::Function &func) {
    _collect_function_info(func);

    _frame_size = 0;

    // size of callee saved registers (include ra if not leaf)
    _frame_size += 8;
    if (!func.is_leaf) {
        _callee_saved_regs_offset[1] = -_frame_size; // ra
    }
    for (auto reg : _callee_saved_regs) {
        _frame_size += 8;
        _callee_saved_regs_offset[reg] = -_frame_size;
    }

    // size of local variables
    for (auto [temp, var] : _local_vars) {
        _frame_size += var.bytes;
        _frame_size = ROUND(_frame_size, var.align);
        _local_var_offset[temp] = -_frame_size;
    }

    // size of spilled registers
    _frame_size = ROUND(_frame_size, 8);
    for (auto temp : _spilled_temps) {
        _frame_size += 8;
        _spilled_temps_offset[temp] = -_frame_size;
    }

    // size of caller saved registers
    for (auto reg : _caller_saved_regs) {
        _frame_size += 8;
        _caller_saved_regs_offset[reg] = -_frame_size;
    }

    // size of outgoing arguments
    if (_max_func_call_args > 8) {
        _frame_size += (_max_func_call_args - 8) * 8;
    }

    // frame size should be aligned to 16 bytes
    _frame_size = ROUND(_frame_size, 16);

    // then use sp instead of fp
    for (auto [reg, offset] : _callee_saved_regs_offset) {
        _callee_saved_regs_offset[reg] = offset + _frame_size;
    }
    for (auto [temp, offset] : _local_var_offset) {
        _local_var_offset[temp] = offset + _frame_size;
    }
    for (auto [temp, offset] : _spilled_temps_offset) {
        _spilled_temps_offset[temp] = offset + _frame_size;
    }
    for (auto [temp, offset] : _caller_saved_regs_offset) {
        _caller_saved_regs_offset[temp] = offset + _frame_size;
    }
}

void StackManager::_collect_function_info(const ir::Function &func) {
    // x8-9, x18-27, f8-9, f18-27
    const std::unordered_set<int> callee_saved_regs = {
        8,       9,       18,      19,      20,      21,      22,      23,
        24,      25,      26,      27,      8 + 32,  9 + 32,  18 + 32, 19 + 32,
        20 + 32, 21 + 32, 22 + 32, 23 + 32, 24 + 32, 25 + 32, 26 + 32, 27 + 32};

    for (auto temp : func.temps_in_func) {
        int reg = temp->reg;
        if (reg == NO_REGISTER) {
            throw std::runtime_error("no register allocated");
        }

        // if spilled
        if (reg == SPILL) {
            _spilled_temps.insert(temp);
            continue;
        }
        // if is s or fs registers
        if (callee_saved_regs.find(reg) != callee_saved_regs.end()) {
            _callee_saved_regs.insert(reg);
        }
    }

    // find allocated stack space for local variables
    for (auto inst : func.start->insts) {
        if (inst->insttype == ir::InstType::IALLOC4 ||
            inst->insttype == ir::InstType::IALLOC8) {
            auto const_val =
                std::static_pointer_cast<ir::ConstBits>(inst->arg[0]);
            auto bytes = std::get<int>(const_val->value);
            int align = inst->insttype == ir::InstType::IALLOC4 ? 4 : 8;
            _local_vars[inst->to] = {align, bytes};
        }
    }

    // find the maximum number of arguments passed to a function
    for (auto block = func.start; block; block = block->next) {
        _collect_block_info(*block);
    }
}

void StackManager::_collect_block_info(const ir::Block &block) {

    // get the maximum number of arguments passed to a function
    int arg_count = 0;
    for (auto inst : block.insts) {
        if (inst->insttype == ir::InstType::IARG) {
            arg_count++;
        } else if (inst->insttype == ir::InstType::ICALL) {
            _max_func_call_args = std::max(_max_func_call_args, arg_count);
            arg_count = 0;
        }
    }

    // get caller saved registers when there is a call
    struct RegReach {
        int reg;
        int end;
    };
    auto asc_end = [](const RegReach &a, const RegReach &b) {
        return a.end <= b.end;
    };
    std::set<RegReach, decltype(asc_end)> reg_reach(asc_end);
    for (auto inst : block.insts) {
        if (inst->insttype == ir::InstType::ICALL) {
            // remove registers that end before the call
            while (reg_reach.size() > 0 &&
                   (*reg_reach.begin()).end <= inst->number) {
                auto front_active = *reg_reach.begin();
                reg_reach.erase(reg_reach.begin());
            }

            // the reset of the registers are caller saved
            for (auto elm : reg_reach) {
                _caller_saved_regs.insert(elm.reg);
            }
        }

        if (inst->to && inst->to->is_local) {
            if (inst->to->reg == NO_REGISTER) {
                throw std::runtime_error("no register allocated");
            } else if (inst->to->reg > 0) {
                reg_reach.insert({inst->to->reg, inst->to->interval.end});
            }
        }
    }
}

} // namespace target
