#pragma once

#include "ir/ir.h"
#include "target/mem.h"
#include "ostream"

namespace target {
    
class Generator {
public:
    Generator() = default;
    Generator(std::ostream &out) : _out(out) {}

    void generate(const ir::Module &module);

    void generate_data(const ir::Data &data);
    void generate_func(const ir::Function &func);

private:
    void _generate_inst(const ir::Inst &inst, StackManager &stack_manager);
    void _generate_call_inst(const ir::Inst &call_inst, const std::vector<ir::ValuePtr> &params, StackManager &stack_manager);

    void _generate_alloc_inst(const ir::Inst &inst, StackManager &stack_manager);
    void _generate_store_inst(const ir::Inst &inst, StackManager &stack_manager);
    void _generate_load_inst(const ir::Inst &inst, StackManager &stack_manager);
    
    void _generate_add_inst(const ir::Inst &inst, StackManager &stack_manager);
    void _generate_sub_inst(const ir::Inst &inst, StackManager &stack_manager);
    void _generate_neg_inst(const ir::Inst &inst, StackManager &stack_manager);
    void _generate_mul_inst(const ir::Inst &inst, StackManager &stack_manager);
    void _generate_div_inst(const ir::Inst &inst, StackManager &stack_manager);
    void _generate_rem_inst(const ir::Inst &inst, StackManager &stack_manager);
    void _generate_copy_inst(const ir::Inst &inst, StackManager &stack_manager);

    void _generate_float_compare_inst(const ir::Inst &inst, StackManager &stack_manager);

    void _generate_extsw_inst(const ir::Inst &inst, StackManager &stack_manager);
    void _generate_stosi_inst(const ir::Inst &inst, StackManager &stack_manager);
    void _generate_swtof_inst(const ir::Inst &inst, StackManager &stack_manager);

    void _generate_par_inst(const ir::Inst &inst, StackManager &stack_manager, int par_count);

    void _generate_jump_inst(const ir::Jump &jump, StackManager &stack_manager);

    std::ostream &_out = std::cout;
};

} // namespace target
