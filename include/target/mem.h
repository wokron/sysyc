#pragma once

#include "ir/ir.h"

namespace target {

/**
 * RISC-V stack frame layout:
 *
 * +-------------------------+ <- fp
 * |    Callee saved regs    |
 * +-------------------------+
 * |    Local variables      |
 * +-------------------------+
 * |    Spilled registers    |
 * +-------------------------+
 * |    Empty align space    |
 * +-------------------------+
 * |    Outgoing arguments   |
 * +-------------------------+ <- sp
 *
 * Note:
 * - s registers should be saved if used in the function
 * - arguments more than 8 should be passed on the stack in reverse order
 * - stack should be aligned to 16 bytes
 */

class StackManager {
public:
    void run(const ir::Function &func);

    int get_frame_size() const { return _frame_size; }

    const std::unordered_map<int, int> &get_callee_saved_regs_offset() const {
        return _callee_saved_regs_offset;
    }

    const std::unordered_map<ir::TempPtr, int> &get_local_var_offset() const {
        return _local_var_offset;
    }

    const std::unordered_map<ir::TempPtr, int> &get_spilled_temps_offset() const {
        return _spilled_temps_offset;
    }

    const std::unordered_map<int, int> &get_caller_saved_regs_offset() const {
        return _caller_saved_regs_offset;
    }

private:
    int _frame_size = 0;
    std::unordered_map<int, int> _callee_saved_regs_offset;
    std::unordered_map<ir::TempPtr, int> _local_var_offset;
    std::unordered_map<ir::TempPtr, int> _spilled_temps_offset;
    std::unordered_map<int, int> _caller_saved_regs_offset;

    // function infos below
    std::unordered_set<int> _callee_saved_regs;
    struct LocalVar {
        int align;
        int bytes;
    };
    std::unordered_map<ir::TempPtr, LocalVar> _local_vars;
    std::unordered_set<ir::TempPtr> _spilled_temps;
    std::unordered_set<int> _caller_saved_regs;
    int _max_func_call_args = 0;

    void _collect_function_info(const ir::Function &func);

    void _collect_block_info(const ir::Block &block);
};

} // namespace target
