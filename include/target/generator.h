#pragma once

#include "ir/ir.h"
#include "ostream"
#include "target/mem.h"
#include "target/peephole.h"
#include <cmath>
#include <functional>
#include <set>

namespace target {

class Generator {
  public:
    Generator() = default;
    Generator(std::ostream &out, bool opt) : _out(out), _opt(opt) {}

    void generate(const ir::Module &module);

    void generate_data(const ir::Data &data);
    void generate_func(const ir::Function &func);

    bool is_power_of_two(int x) { return x > 0 && (x & (x - 1)) == 0; }

    int calculate_exponent(int x) { return (int)(log(x) / log(2)); }

    bool is_power_of_two_all(int x) {
        return is_power_of_two(x) || is_power_of_two(x + 1) ||
               is_power_of_two(x - 1);
    }

    std::pair<uint64_t, int> choose_pair(int d, int prec) {
        uint64_t nc = (1ULL << prec) - ((1ULL << prec) % d) - 1;
        uint64_t p = 32;
        while ((1ULL << p) <= nc * (d - (1ULL << p) % d)) {
            p++;
        }
        uint64_t m = (((1ULL << p) + d - (1ULL << p) % d) / d);
        uint64_t n = ((m << 32) >> 32);
        return std::make_pair(n, static_cast<int>(p - 32));
    }

    int getCTZ(int num) {
        unsigned int r = 0;
        unsigned int unsigned_num =
            static_cast<unsigned int>(num); 

        unsigned_num >>= 1; 

        while (unsigned_num > 0) {
            r++;
            unsigned_num >>= 1;
        }

        return r;
    }

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

    std::string _get_asm_arg(
        ir::ValuePtr arg, int no,
        std::function<int(ir::Type, int)> get_temp_reg = _get_temp_reg);

    std::tuple<std::string, bool> _get_asm_arg_or_w_constbits(ir::ValuePtr arg,
                                                              int no);
    std::string _get_asm_addr(ir::ValuePtr addr, int no);

    std::tuple<std::string, std::function<void(std::ostream &)>>
    _get_asm_to(ir::TempPtr to,
                std::function<int(ir::Type, int)> get_temp_reg = _get_temp_reg);

    static int _get_temp_reg(ir::Type type, int no);

    std::ostream &_out = std::cout;
    StackManager _stack_manager;
    std::vector<ir::DataPtr> _local_data;

    struct RegReach {
        int reg;
        int end;
    };
    std::set<RegReach, std::function<bool(const RegReach &, const RegReach &)>>
        _reg_reach;

    bool _opt;
    PeepholeBuffer _buffer;
};

} // namespace target
