#pragma once

#include "ir/ir.h"
#include "target/mem.h"
#include "ostream"
#include "target/mem.h"
#include <functional>
#include <set>

namespace target {

class Generator {
public:
    Generator() = default;
    Generator(std::ostream &out) : _out(out) {}

    void generate(const ir::Module &module);

    void generate_data(const ir::Data &data);
    void generate_func(const ir::Function &func);

private:
    void _generate_inst(const ir::Inst &inst);

    void _generate_load_inst(const ir::Inst &inst);
    void _generate_store_inst(const ir::Inst &inst);
    void _generate_arithmetic_inst(const ir::Inst &inst);
    void _generate_compare_inst(const ir::Inst &inst);
    void _generate_float_compare_inst(const ir::Inst &inst);
    void _generate_unary_inst(const ir::Inst &inst);
    void _generate_convert_inst(const ir::Inst &inst);

    void _generate_call_inst(const ir::Inst &inst,
                             const std::vector<ir::ValuePtr> &args);
    void _generate_arguments(const std::vector<ir::ValuePtr> &args, int pass);
    void _generate_par_inst(const ir::Inst &inst, int par_count);
    void _generate_jump_inst(const ir::Jump &jump);

    std::string _get_asm_arg(ir::ValuePtr arg, int no);
    std::tuple<std::string, bool>
    _get_asm_arg_or_w_constbits(ir::ValuePtr arg, int no);
    std::string _get_asm_addr(ir::ValuePtr addr, int no);
    std::tuple<std::string, std::function<void(std::ostream &)>>
    _get_asm_to(ir::TempPtr to);

    int _get_temp_reg(ir::Type type, int no);

    std::ostream &_out = std::cout;
    StackManager _stack_manager;
    std::vector<ir::DataPtr> _local_data;

    struct RegReach {
        int reg;
        int end;
    };
    std::set<RegReach, std::function<bool(const RegReach &, const RegReach &)>>
        _reg_reach;
};

} // namespace target
