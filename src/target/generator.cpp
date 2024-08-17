#include "target/generator.h"
#include "array"
#include "target/mem.h"
#include "target/regalloc.h"
#include "target/utils.h"

namespace target {

#define INDENT "    "

void Generator::generate(const ir::Module &module) {
    for (const auto &data : module.datas) {
        generate_data(*data);
    }

    for (const auto &func : module.functions) {
        generate_func(*func);
    }

    for (const auto &data : _local_data) {
        generate_data(*data);
    }

    _out << ".section .note.GNU-stack,\"\",@progbits" << std::endl;
}

void Generator::generate_data(const ir::Data &data) {
    bool is_bss = data.items.size() == 1 && dynamic_cast<const ir::ZeroData *>(
                                                data.items[0].get()) != nullptr;

    if (is_bss) {
        _out << ".bss " << std::endl;
    } else {
        _out << ".data " << std::endl;
    }

    _out << ".balign " << data.align << std::endl;

    if (data.is_export) {
        _out << ".global " << data.name << std::endl;
    }

    _out << data.name << ":" << std::endl;

    for (const auto &item : data.items) {
        if (auto zero_data = dynamic_cast<const ir::ZeroData *>(item.get())) {
            _out << ".zero " << zero_data->bytes << std::endl;
        } else if (auto const_data =
                       dynamic_cast<const ir::ConstData *>(item.get())) {
            std::string asm_type;
            switch (const_data->ty) {
            case ir::Type::W:
            case ir::Type::S:
                asm_type = ".word";
                break;
            case ir::Type::L:
                asm_type = ".quad";
                break;
            default:
                throw std::logic_error("unsupported data type");
                break;
            }

            for (const auto &value : const_data->values) {
                _out << INDENT << asm_type << " " << value->get_asm_value()
                     << std::endl;
            }
        } else {
            throw std::logic_error("unknown data item type");
        }
    }

    _out << ".type " << data.name << ", @object" << std::endl;
    _out << "/* end data " << data.name << " */" << std::endl << std::endl;
}

void Generator::generate_func(const ir::Function &func) {
    LinearScanAllocator regalloc;
    regalloc.allocate_registers(func);

    bool minimum_stack = true;

    _stack_manager = StackManager();
    _stack_manager.run(func);

    _buffer.clear();

    _out << ".text" << std::endl;

    if (func.is_export) {
        _out << ".global " << func.name << std::endl;
    }

    _out << func.name << ":" << std::endl;

    int frame_size = _stack_manager.get_frame_size();
    if (frame_size > 16) {
        minimum_stack = false;
    }

    if (is_in_imm12_range(frame_size)) {
        _buffer.append("addi", "sp", "sp", std::to_string(-frame_size))
            .set_entry();
    } else { // since a5-a6 may still store value here, we use t0 as
             // intermediate reg
        _buffer.append("li", "t0", std::to_string(frame_size)).set_entry();
        _buffer.append("sub", "sp", "sp", "t0").set_entry();
    }

    for (auto [reg, offset] : _stack_manager.get_callee_saved_regs_offset()) {
        std::string store = (reg >= 32 ? "fsd" : "sd");
        if (reg != 1) {
            minimum_stack = false;
        }
        if (is_in_imm12_range(offset)) {
            auto &inst = _buffer.append(store, regno2string(reg),
                                        std::to_string(offset) + "(sp)");
            if (reg == 1) { // ra
                inst.set_entry();
            }
        } else {
            _buffer.append("li", "t0", std::to_string(offset));
            _buffer.append("add", "t0", "sp", "t0");
            _buffer.append(store, regno2string(reg), "0(t0)");
        }
    }

    for (auto block = func.start; block; block = block->next) {
        _reg_reach =
            decltype(_reg_reach)([](const RegReach &a, const RegReach &b) {
                return a.end <= b.end;
            });

        _buffer.append(".L" + std::to_string(block->id));

        std::vector<ir::ValuePtr> call_args;
        int par_count = 0;
        for (const auto &inst : block->insts) {
            if (inst->insttype == ir::InstType::IARG) {
                call_args.push_back(inst->arg[0]);
            } else if (inst->insttype == ir::InstType::ICALL) {
                _generate_call_inst(*inst, call_args);
                call_args.clear();
            } else if (inst->insttype == ir::InstType::IPAR) {
                _generate_par_inst(*inst, par_count++);
            } else {
                _generate_inst(*inst);
            }

            if (inst->to && inst->to->is_local) {
                if (inst->to->reg == NO_REGISTER) {
                    throw std::runtime_error("no register allocated");
                } else if (inst->to->reg > 0) {
                    _reg_reach.insert({inst->to->reg, inst->to->interval.end});
                }
            }
        }
        _generate_jump_inst(block->jump);
    }

    if (_opt) {
        _buffer.optimize(minimum_stack);
    }

    _buffer.emit(_out);

    _out << ".type " << func.name << ", @function" << std::endl;
    _out << ".size " << func.name << ", .-" << func.name << std::endl;
    _out << "/* end function " << func.name << " */" << std::endl << std::endl;
}

void Generator::_generate_inst(const ir::Inst &inst) {
    switch (inst.insttype) {
    case ir::InstType::ISTOREW:
    case ir::InstType::ISTOREL:
    case ir::InstType::ISTORES:
        _generate_store_inst(inst);
        break;
    case ir::InstType::ILOADW:
    case ir::InstType::ILOADL:
    case ir::InstType::ILOADS:
        _generate_load_inst(inst);
        break;
    case ir::InstType::IADD:
    case ir::InstType::ISUB:
    case ir::InstType::IMUL:
    case ir::InstType::IDIV:
    case ir::InstType::IREM:
        _generate_arithmetic_inst(inst);
        break;
    case ir::InstType::ICEQW:
    case ir::InstType::ICNEW:
    case ir::InstType::ICSLEW:
    case ir::InstType::ICSLTW:
    case ir::InstType::ICSGEW:
    case ir::InstType::ICSGTW:
        _generate_compare_inst(inst);
        break;
    case ir::InstType::ICEQS:
    case ir::InstType::ICNES:
    case ir::InstType::ICLES:
    case ir::InstType::ICLTS:
    case ir::InstType::ICGES:
    case ir::InstType::ICGTS:
        _generate_float_compare_inst(inst);
        break;
    case ir::InstType::ICOPY:
    case ir::InstType::INEG:
        _generate_unary_inst(inst);
        break;
    case ir::InstType::IEXTSW:
    case ir::InstType::ISTOSI:
    case ir::InstType::ISWTOF:
        _generate_convert_inst(inst);
        break;
    case ir::InstType::IALLOC4:
    case ir::InstType::IALLOC8:
    case ir::InstType::INOP:
        /* nop */
        break;
    default:
        throw std::logic_error("unsupported instruction");
    }
}

void Generator::_generate_load_inst(const ir::Inst &inst) {
    static std::unordered_map<ir::InstType, std::string> inst2asm = {
        {ir::InstType::ILOADW, "lw"},
        {ir::InstType::ILOADL, "ld"},
        {ir::InstType::ILOADS, "flw"},
    };

    auto [to, write_back] = _get_asm_to(inst.to);
    auto arg = _get_asm_addr(inst.arg[0], 0);

    _buffer.append(inst2asm.at(inst.insttype), to, arg);

    write_back(_out);
}

void Generator::_generate_store_inst(const ir::Inst &inst) {
    static std::unordered_map<ir::InstType, std::string> inst2asm = {
        {ir::InstType::ISTOREW, "sw"},
        {ir::InstType::ISTOREL, "sd"},
        {ir::InstType::ISTORES, "fsw"},
    };

    if (auto const_arg0 =
            std::dynamic_pointer_cast<ir::ConstBits>(inst.arg[0])) {
        if (auto int_value = std::get_if<int>(&const_arg0->value);
            int_value != nullptr && *int_value == 0) {
            _buffer.append("sw", "zero", _get_asm_addr(inst.arg[1], 1));
            return;
        } else if (auto float_value = std::get_if<float>(&const_arg0->value);
                   float_value != nullptr && *float_value == 0.0f) {
            _buffer.append("sw", "zero", _get_asm_addr(inst.arg[1], 1));
            return;
        }
    }

    auto arg0 = _get_asm_arg(inst.arg[0], 0);
    auto arg1 = _get_asm_addr(inst.arg[1], 1);

    _buffer.append(inst2asm.at(inst.insttype), arg0, arg1);
}

void Generator::_generate_arithmetic_inst(const ir::Inst &inst) {
    // clang-format off
    static std::unordered_map<ir::InstType, std::unordered_map<ir::Type, std::string>> inst2asm = {
        {ir::InstType::IADD, {{ir::Type::L, "add"}, {ir::Type::W, "addw"}, {ir::Type::S, "fadd.s"}}},
        {ir::InstType::ISUB, {{ir::Type::L, "sub"}, {ir::Type::W, "subw"}, {ir::Type::S, "fsub.s"}}},
        {ir::InstType::IMUL, {{ir::Type::L, "mul"}, {ir::Type::W, "mulw"}, {ir::Type::S, "fmul.s"}}},
        {ir::InstType::IDIV, {{ir::Type::L, "div"}, {ir::Type::W, "divw"}, {ir::Type::S, "fdiv.s"}}},
        {ir::InstType::IREM, {{ir::Type::L, "rem"}, {ir::Type::W, "remw"}}},
    };
    // clang-format on

    if (inst.to->reg == STACK) {
        return;
    }

    auto [to, write_back] = _get_asm_to(inst.to);

    auto inst_str = inst2asm.at(inst.insttype).at(inst.to->get_type());
    bool wflag = inst_str == "mulw" || inst_str == "divw" || inst_str == "remw";
    if (inst_str == "mulw" || inst_str == "mul") {
        if (auto constbits =
                std::dynamic_pointer_cast<ir::ConstBits>(inst.arg[0])) {
            if (auto value = std::get_if<int>(&constbits->value)) {
                auto arg1 = _get_asm_arg(inst.arg[1], 1);
                if (is_power_of_two(*value)) {
                    _buffer.append(wflag ? "slliw" : "slli", to, arg1,
                                   std::to_string(calculate_exponent(*value)));
                } else if (is_power_of_two(*value + 1)) {
                    _buffer.append(
                        wflag ? "slliw" : "slli", "a5", arg1,
                        std::to_string(calculate_exponent(*value + 1)));
                    _buffer.append(wflag ? "subw" : "sub", to, "a5", arg1);
                } else if (is_power_of_two(*value - 1)) {
                    _buffer.append(
                        wflag ? "slliw" : "slli", "a5", arg1,
                        std::to_string(calculate_exponent(*value - 1)));
                    _buffer.append(wflag ? "addw" : "add", to, "a5", arg1);
                } else {
                    auto arg0 = _get_asm_arg(inst.arg[0], 0);
                    _buffer.append(inst_str, to, arg0, arg1);
                }
            } else {
                auto arg0 = _get_asm_arg(inst.arg[0], 0);
                auto arg1 = _get_asm_arg(inst.arg[1], 1);
                _buffer.append(inst_str, to, arg0, arg1);
            }
        } else if (auto constbits =
                       std::dynamic_pointer_cast<ir::ConstBits>(inst.arg[1])) {
            if (auto value = std::get_if<int>(&constbits->value)) {
                auto arg0 = _get_asm_arg(inst.arg[0], 0);
                if (is_power_of_two(*value)) {
                    _buffer.append(wflag ? "slliw" : "slli", to, arg0,
                                   std::to_string(calculate_exponent(*value)));
                } else if (is_power_of_two(*value + 1)) {
                    _buffer.append(
                        wflag ? "slliw" : "slli", "a5", arg0,
                        std::to_string(calculate_exponent(*value + 1)));
                    _buffer.append(wflag ? "subw" : "sub", to, "a5", arg0);
                } else if (is_power_of_two(*value - 1)) {
                    _buffer.append(
                        wflag ? "slliw" : "slli", "a5", arg0,
                        std::to_string(calculate_exponent(*value - 1)));
                    _buffer.append(wflag ? "addw" : "add", to, "a5", arg0);
                } else {
                    auto arg1 = _get_asm_arg(inst.arg[1], 1);
                    _buffer.append(inst_str, to, arg0, arg1);
                }
            } else {
                auto arg0 = _get_asm_arg(inst.arg[0], 0);
                auto arg1 = _get_asm_arg(inst.arg[1], 1);
                _buffer.append(inst_str, to, arg0, arg1);
            }
        } else {
            auto arg0 = _get_asm_arg(inst.arg[0], 0);
            auto arg1 = _get_asm_arg(inst.arg[1], 1);
            _buffer.append(inst_str, to, arg0, arg1);
        }
    } else if (inst_str == "divw") {
        if (auto constbits =
                std::dynamic_pointer_cast<ir::ConstBits>(inst.arg[1])) {
            if (auto value = std::get_if<int>(&constbits->value)) {
                auto arg0 = _get_asm_arg(inst.arg[0], 0);
                int value_num = *value;
                int abs = value_num;
                if (abs < 0) {
                    abs = -abs;
                }
                if ((abs & (abs - 1)) == 0) {
                    _buffer.append(wflag ? "sraiw" : "srai", "a5", arg0, "31");
                    int l = getCTZ(abs);
                    _buffer.append(wflag ? "srliw" : "srli", "a5", "a5",
                                   std::to_string(32 - l));
                    _buffer.append(wflag ? "addw" : "add", "a5", "a5", arg0);
                    _buffer.append(wflag ? "sraiw" : "srai", "a5", "a5",
                                   std::to_string(l));
                } else {
                    auto res = choose_pair(abs, 31);
                    int64_t m = res.first;
                    int sh = res.second;
                    if (m < 2147483648ULL) {
                        _buffer.append("li", "a5", std::to_string(m));
                        _buffer.append("mul", "a5", arg0, "a5");
                        _buffer.append("srli", "a5", "a5", "32");
                    } else {
                        _buffer.append("li", "a5",
                                       std::to_string(m - (1ULL << 32)));
                        _buffer.append("mul", "a5", arg0, "a5");
                        _buffer.append("srli", "a5", "a5", "32");
                        _buffer.append("addw", "a5", arg0, "a5");
                    }
                    _buffer.append(wflag ? "sraiw" : "srai", "a5", "a5",
                                   std::to_string(sh));
                    _buffer.append(wflag ? "srliw" : "srli", "a6", arg0,
                                   std::to_string(31));
                    _buffer.append(wflag ? "addw" : "add", "a5", "a5", "a6");
                }
                if (value_num < 0) {
                    _buffer.append(wflag ? "subw" : "sub", "a5", "zero", "a5");
                }
                _buffer.append(wflag ? "addw" : "add", to, "zero", "a5");
            } else {
                auto arg0 = _get_asm_arg(inst.arg[0], 0);
                auto arg1 = _get_asm_arg(inst.arg[1], 1);
                _buffer.append(inst_str, to, arg0, arg1);
            }
        } else {
            auto arg0 = _get_asm_arg(inst.arg[0], 0);
            auto arg1 = _get_asm_arg(inst.arg[1], 1);
            _buffer.append(inst_str, to, arg0, arg1);
        }
    } else if (inst_str == "remw") {
        if (auto constbits =
                std::dynamic_pointer_cast<ir::ConstBits>(inst.arg[1])) {
            if (auto value = std::get_if<int>(&constbits->value)) {
                auto arg0 = _get_asm_arg(inst.arg[0], 0);
                int value_num = *value;
                int abs = value_num;
                if (abs < 0) {
                    abs = -abs;
                }
                if ((abs & (abs - 1)) == 0) {
                    _buffer.append(wflag ? "sraiw" : "srai", "a5", arg0, "31");
                    int l = getCTZ(abs);
                    _buffer.append(wflag ? "srliw" : "srli", "a5", "a5",
                                   std::to_string(32 - l));
                    _buffer.append(wflag ? "addw" : "add", "a5", "a5", arg0);
                    _buffer.append(wflag ? "sraiw" : "srai", "a5", "a5",
                                   std::to_string(l));
                } else {
                    auto res = choose_pair(abs, 31);
                    int64_t m = res.first;
                    int sh = res.second;
                    if (m < 2147483648ULL) {
                        _buffer.append("li", "a5", std::to_string(m));
                        _buffer.append("mul", "a5", arg0, "a5");
                        _buffer.append("srli", "a5", "a5", "32");
                    } else {
                        _buffer.append("li", "a5",
                                       std::to_string(m - (1ULL << 32)));
                        _buffer.append("mul", "a5", arg0, "a5");
                        _buffer.append("srli", "a5", "a5", "32");
                        _buffer.append("addw", "a5", arg0, "a5");
                    }
                    _buffer.append(wflag ? "sraiw" : "srai", "a5", "a5",
                                   std::to_string(sh));
                    _buffer.append(wflag ? "srliw" : "srli", "a6", arg0,
                                   std::to_string(31));
                    _buffer.append(wflag ? "addw" : "add", "a5", "a5", "a6");
                }
                if (value_num < 0) {
                    _buffer.append(wflag ? "subw" : "sub", "a5", "zero", "a5");
                }
                _buffer.append("li", "a6", std::to_string(value_num));
                _buffer.append(wflag ? "mulw" : "mul", "a5", "a6", "a5");
                _buffer.append(wflag ? "subw" : "sub", to, arg0, "a5");
            } else {
                auto arg0 = _get_asm_arg(inst.arg[0], 0);
                auto arg1 = _get_asm_arg(inst.arg[1], 1);
                _buffer.append(inst_str, to, arg0, arg1);
            }
        } else {
            auto arg0 = _get_asm_arg(inst.arg[0], 0);
            auto arg1 = _get_asm_arg(inst.arg[1], 1);
            _buffer.append(inst_str, to, arg0, arg1);
        }
    } else {
        auto arg0 = _get_asm_arg(inst.arg[0], 0);
        auto arg1 = _get_asm_arg(inst.arg[1], 1);
        _buffer.append(inst_str, to, arg0, arg1);
    }

    write_back(_out);
}

int getCTZ(int num) {
    int r = 0;
    num >>= 1;
    while (num > 0) {
        r++;
        num >>= 1;
    }
    return r;
}

void Generator::_generate_compare_inst(const ir::Inst &inst) {
    auto [to, write_back] = _get_asm_to(inst.to);
    auto arg0 = _get_asm_arg(inst.arg[0], 0);
    auto arg1 = _get_asm_arg(inst.arg[1], 1);

    switch (inst.insttype) {
    case ir::InstType::ICEQW: // (a0 ^ a1) < 1
        _buffer.append("xor", to, arg0, arg1);
        _buffer.append("sltiu", to, to, "1");
        break;
    case ir::InstType::ICNEW: // 0 < (a0 ^ a1)
        _buffer.append("xor", to, arg0, arg1);
        _buffer.append("sltu", to, "zero", to);
        break;
    case ir::InstType::ICSLEW: // !(a1 < a0)
        _buffer.append("slt", to, arg1, arg0);
        _buffer.append("xori", to, to, "1");
        break;
    case ir::InstType::ICSLTW: // a0 < a1
        _buffer.append("slt", to, arg0, arg1);
        break;
    case ir::InstType::ICSGEW: // !(a0 < a1)
        _buffer.append("slt", to, arg0, arg1);
        _buffer.append("xori", to, to, "1");
        break;
    case ir::InstType::ICSGTW: // a1 < a0
        _buffer.append("slt", to, arg1, arg0);
        break;
    default:
        throw std::logic_error("unsupported type");
    }

    write_back(_out);
}

void Generator::_generate_float_compare_inst(const ir::Inst &inst) {
    static std::unordered_map<ir::InstType, std::string> inst2asm = {
        {ir::InstType::ICEQS, "feq.s"}, {ir::InstType::ICNES, "feq.s"},
        {ir::InstType::ICLES, "fle.s"}, {ir::InstType::ICLTS, "flt.s"},
        {ir::InstType::ICGES, "fle.s"}, {ir::InstType::ICGTS, "flt.s"},
    };

    auto [to, write_back] = _get_asm_to(inst.to);
    auto arg0 = _get_asm_arg(inst.arg[0], 0);
    auto arg1 = _get_asm_arg(inst.arg[1], 1);

    if (inst.insttype == ir::InstType::ICGES ||
        inst.insttype == ir::InstType::ICGTS) {
        std::swap(arg0, arg1);
    }
    _buffer.append(inst2asm.at(inst.insttype), to, arg0, arg1);
    if (inst.insttype == ir::InstType::ICNES) {
        _buffer.append("xori", to, to, "1");
    }

    write_back(_out);
}

void Generator::_generate_unary_inst(const ir::Inst &inst) {
    // clang-format off
    static std::unordered_map<ir::InstType, std::unordered_map<ir::Type, std::string>> inst2asm = {
        {ir::InstType::ICOPY, {{ir::Type::L, "mv"}, {ir::Type::W, "mv"}, {ir::Type::S, "fmv.s"}}},
        {ir::InstType::INEG, {{ir::Type::L, "neg"}, {ir::Type::W, "negw"}, {ir::Type::S, "fneg.s"}}},
    };
    // clang-format on

    auto [to, write_back] = _get_asm_to(inst.to);
    auto arg = _get_asm_arg(inst.arg[0], 0);

    auto inst_str = inst2asm.at(inst.insttype).at(inst.to->get_type());

    _buffer.append(inst_str, to, arg);

    write_back(_out);
}

void Generator::_generate_convert_inst(const ir::Inst &inst) {
    static std::unordered_map<ir::InstType, std::string> inst2asm = {
        {ir::InstType::IEXTSW, "sext.w"},
        {ir::InstType::ISTOSI, "fcvt.w.s"},
        {ir::InstType::ISWTOF, "fcvt.s.w"},
    };

    auto [to, write_back] = _get_asm_to(inst.to);
    auto arg = _get_asm_arg(inst.arg[0], 0);

    if (inst.insttype == ir::InstType::ISTOSI) {
        _buffer.append(inst2asm.at(inst.insttype), to, arg, "rtz");
    } else {
        _buffer.append(inst2asm.at(inst.insttype), to, arg);
    }

    write_back(_out);
}

void Generator::_generate_call_inst(const ir::Inst &inst,
                                    const std::vector<ir::ValuePtr> &args) {

    while (_reg_reach.size() > 0 && (*_reg_reach.begin()).end <= inst.number) {
        auto front_active = *_reg_reach.begin();
        _reg_reach.erase(_reg_reach.begin());
    }

    // save caller saved registers (t and ft)
    for (auto [reg, end] : _reg_reach) {
        auto offset = _stack_manager.get_caller_saved_regs_offset().at(reg);
        auto store = reg >= 32 ? "fsd" : "sd";
        if (is_in_imm12_range(offset)) {
            _buffer.append(store, regno2string(reg),
                           std::to_string(offset) + "(sp)");
        } else {
            _buffer.append("li", "a5", std::to_string(offset));
            _buffer.append("add", "a5", "sp", "a5");
            _buffer.append(store, regno2string(reg), "0(a5)");
        }
    }

    int arg_count = args.size() - 1;
    for (auto it = args.rbegin(); it != args.rend(); it++, arg_count--) {
        auto arg = *it;
        if (arg_count <= 7) {
            std::function<int(ir::Type, int)> get_temp_reg = _get_temp_reg;
            if (arg_count < 4) {
                get_temp_reg = [](ir::Type type, int no) {
                    std::array<int, 2> reg = {10, 11};            // a0, a1
                    std::array<int, 2> freg = {32 + 10, 32 + 11}; // fa0, fa1
                    if (type == ir::Type::S) {
                        return freg[no];
                    } else {
                        return reg[no];
                    }
                };
            }
            auto arg0 = _get_asm_arg(arg, 0, get_temp_reg);
            switch (arg->get_type()) {
            case ir::Type::W:
            case ir::Type::L:
                _buffer.append("mv", "a" + std::to_string(arg_count), arg0);
                break;
            case ir::Type::S:
                _buffer.append("fmv.s", "fa" + std::to_string(arg_count), arg0);
                break;
            default:
                throw std::logic_error("unsupported type");
            }
        } else {
            static std::unordered_map<ir::Type, std::string> inst2asm = {
                {ir::Type::W, "sw"}, {ir::Type::L, "sd"}, {ir::Type::S, "fsw"}};
            int offset = (arg_count - 8) * 8;
            auto arg0 = _get_asm_arg(arg, 0);
            if (is_in_imm12_range(offset)) {
                _buffer.append(inst2asm.at(arg->get_type()), arg0,
                               std::to_string(offset) + "(sp)");
            } else {
                _buffer.append("li", "a5", std::to_string(offset));
                _buffer.append("add", "a5", "sp", "a5");
                _buffer.append(inst2asm.at(arg->get_type()), arg0, "0(a5)");
            }
        }
    }

    _buffer.append("call",
                   std::static_pointer_cast<ir::Address>(inst.arg[0])->name);

    if (inst.to != nullptr && inst.to->uses.size() > 0) {
        auto [to, write_back] = _get_asm_to(inst.to);
        switch (inst.to->get_type()) {
        case ir::Type::W:
        case ir::Type::L:
            _buffer.append("mv", to, "a0");
            break;
        case ir::Type::S:
            _buffer.append("fmv.s", to, "fa0");
            break;
        default:
            throw std::logic_error("unsupported type");
        }
        write_back(_out);
    }

    // restore caller saved registers
    for (auto [reg, end] : _reg_reach) {
        auto offset = _stack_manager.get_caller_saved_regs_offset().at(reg);
        auto load = reg >= 32 ? "fld" : "ld";
        if (is_in_imm12_range(offset)) {
            _buffer.append(load, regno2string(reg),
                           std::to_string(offset) + "(sp)");
        } else {
            _buffer.append("li", "a5", std::to_string(offset));
            _buffer.append("add", "a5", "sp", "a5");
            _buffer.append(load, regno2string(reg), "0(a5)");
        }
    }
}

void Generator::_generate_arguments(const std::vector<ir::ValuePtr> &args,
                                    int pass) {
    int arg_count = args.size() - 1;
    for (auto it = args.rbegin(); it != args.rend(); it++, arg_count--) {
        auto arg = *it;
        if (arg_count <= 7) {
            if ((arg->get_type() == ir::Type::W) && pass == 2) {
                auto [arg0, is_const] = _get_asm_arg_or_w_constbits(arg, 0);
                std::string inst = is_const ? "li" : "mv";
                _buffer.append(inst, "a" + std::to_string(arg_count), arg0);
            } else if ((arg->get_type() == ir::Type::L) && pass == 0) {
                auto arg0 = _get_asm_arg(arg, 0);
                _buffer.append("mv", "a" + std::to_string(arg_count), arg0);
            } else if ((arg->get_type() == ir::Type::S) &&
                       (((pass == 0) && (arg_count != 4)) ||
                        ((pass == 1) && (arg_count == 4)))) {
                auto arg0 = _get_asm_arg(arg, 0);
                _buffer.append("fmv.s", "fa" + std::to_string(arg_count), arg0);
            }
        } else if (pass == 0) {
            static std::unordered_map<ir::Type, std::string> inst2asm = {
                {ir::Type::W, "sw"}, {ir::Type::L, "sd"}, {ir::Type::S, "fsw"}};
            int offset = (arg_count - 8) * 8;
            auto arg0 = _get_asm_arg(arg, 0);
            if (is_in_imm12_range(offset)) {
                _buffer.append(inst2asm.at(arg->get_type()), arg0,
                               std::to_string(offset) + "(sp)");
            } else {
                _buffer.append("li", "a5", std::to_string(offset));
                _buffer.append("add", "a5", "sp", "a5");
                _buffer.append(inst2asm.at(arg->get_type()), arg0, "0(a5)");
            }
        }
    }
}

void Generator::_generate_par_inst(const ir::Inst &inst, int par_count) {
    std::function<int(ir::Type, int)> get_temp_reg = _get_temp_reg;
    if (par_count < 4) {
        get_temp_reg = [](ir::Type type, int no) {
            std::array<int, 2> reg = {10, 11};            // a0, a1
            std::array<int, 2> freg = {32 + 10, 32 + 11}; // fa0, fa1
            if (type == ir::Type::S) {
                return freg[no];
            } else {
                return reg[no];
            }
        };
    }
    auto [to, write_back] = _get_asm_to(inst.to, get_temp_reg);
    if (par_count <= 7) { // in a0-a7
        switch (inst.to->get_type()) {
        case ir::Type::L:
        case ir::Type::W:
            _buffer.append("mv", to, "a" + std::to_string(par_count));
            break;
        case ir::Type::S:
            _buffer.append("fmv.s", to, "fa" + std::to_string(par_count));
            break;
        default:
            break; // unreachable
        }
    } else {
        static std::unordered_map<ir::Type, std::string> inst2asm = {
            {ir::Type::W, "lw"}, {ir::Type::L, "ld"}, {ir::Type::S, "flw"}};
        int offset = (par_count - 8) * 8 + _stack_manager.get_frame_size();
        if (is_in_imm12_range(offset)) {
            _buffer.append(inst2asm.at(inst.to->get_type()), to,
                           std::to_string(offset) + "(sp)");
        } else {
            _buffer.append("li", "a5", std::to_string(offset));
            _buffer.append("add", "a5", "sp", "a5");
            _buffer.append(inst2asm.at(inst.to->get_type()), to, "0(a5)");
        }
    }
    write_back(_out);
}

void Generator::_generate_jump_inst(const ir::Jump &jump) {
    switch (jump.type) {
    case ir::Jump::NONE:
        return;
    case ir::Jump::RET: {
        if (jump.arg) {
            auto arg = _get_asm_arg(jump.arg, 0);
            if (jump.arg->get_type() == ir::Type::S) {
                _buffer.append("fmv.s", "fa0", arg);
            } else {
                _buffer.append("mv", "a0", arg);
            }
        }

        // recover saved registers
        for (auto [reg, offset] :
             _stack_manager.get_callee_saved_regs_offset()) {
            std::string load = (reg >= 32 ? "fld" : "ld");
            if (is_in_imm12_range(offset)) {
                auto &inst = _buffer.append(load, regno2string(reg),
                                            std::to_string(offset) + "(sp)");
                if (reg == 1) {
                    inst.set_exit();
                }
            } else {
                _buffer.append("li", "a5", std::to_string(offset));
                _buffer.append("add", "a5", "sp", "a5");
                _buffer.append(load, regno2string(reg), "0(a5)");
            }
        }

        auto frame_size = _stack_manager.get_frame_size();
        if (is_in_imm12_range(frame_size)) {
            _buffer.append("addi", "sp", "sp", std::to_string(frame_size))
                .set_exit();
        } else {
            _buffer.append("li", "a5", std::to_string(frame_size)).set_exit();
            _buffer.append("add", "sp", "sp", "a5").set_exit();
        }

        _buffer.append("jr", "ra");
    } break;
    case ir::Jump::JMP:
        _buffer.append("j", ".L" + std::to_string(jump.blk[0]->id));
        break;
    case ir::Jump::JNZ: {
        auto arg = _get_asm_arg(jump.arg, 0);
        _buffer.append("bnez", arg, ".L" + std::to_string(jump.blk[0]->id));
        _buffer.append("j", ".L" + std::to_string(jump.blk[1]->id));
    } break;
    default:
        throw std::logic_error("unsupported jump type");
    }
}

static bool is_int_compare(ir::InstType insttype) {
    switch (insttype) {
    case ir::InstType::ICEQW:
    case ir::InstType::ICNEW:
    case ir::InstType::ICSLEW:
    case ir::InstType::ICSLTW:
    case ir::InstType::ICSGEW:
    case ir::InstType::ICSGTW:
        return true;
    default:
        return false;
    }
}

std::string
Generator::_get_asm_arg(ir::ValuePtr arg, int no,
                        std::function<int(ir::Type, int)> get_temp_reg) {
    if (auto temp = std::dynamic_pointer_cast<ir::Temp>(arg)) {
        if (temp->reg >= 0) {
            return regno2string(temp->reg);
        }
        if (temp->reg == SPILL) {
            std::string load;
            switch (temp->get_type()) {
            case ir::Type::W:
                load = "lw";
                break;
            case ir::Type::L:
                load = "ld";
                break;
            case ir::Type::S:
                load = "flw";
                break;
            default:
                throw std::logic_error("unsupported type");
            };
            int offset = _stack_manager.get_spilled_temps_offset().at(temp);
            int reg = get_temp_reg(temp->get_type(), no);
            std::string reg_str = regno2string(reg);

            if (is_in_imm12_range(offset)) {
                _buffer.append(load, reg_str, std::to_string(offset) + "(sp)");
            } else {
                _buffer.append("li", "a5", std::to_string(offset));
                _buffer.append("add", "a5", "sp", "a5");
                _buffer.append(load, reg_str, "0(a5)");
            }

            return reg_str;
        } else if (temp->reg == STACK) {
            int offset;
            if (auto def_inst = std::get<ir::InstDef>(temp->defs[0]).ins;
                def_inst->insttype == ir::InstType::IADD) {
                auto alloc_temp =
                    std::static_pointer_cast<ir::Temp>(def_inst->arg[0]);
                auto add_const = std::get<int>(
                    std::static_pointer_cast<ir::ConstBits>(def_inst->arg[1])
                        ->value);
                offset = _stack_manager.get_local_var_offset().at(alloc_temp) +
                         add_const;
            } else {
                offset = _stack_manager.get_local_var_offset().at(temp);
            }
            int reg = get_temp_reg(temp->get_type(), no);
            std::string reg_str = regno2string(reg);

            if (is_in_imm12_range(offset)) {
                _buffer.append("addi", reg_str, "sp", std::to_string(offset));
            } else {
                _buffer.append("li", reg_str, std::to_string(offset));
                _buffer.append("add", reg_str, "sp", reg_str);
            }

            return reg_str;
        } else {
            throw std::logic_error("no register");
        }
    } else if (auto constbits = std::dynamic_pointer_cast<ir::ConstBits>(arg)) {
        int reg = get_temp_reg(constbits->get_type(), no);
        std::string reg_str = regno2string(reg);

        if (constbits->get_type() == ir::Type::S) {
            if (constbits->get_asm_value() == "0x0") {
                _buffer.append("fmv.w.x", reg_str, "zero");
            } else {
                std::string local_data_name =
                    ".LC" + std::to_string(_local_data.size());

                auto local_data = std::shared_ptr<ir::Data>(
                    new ir::Data{false, local_data_name, 4});
                local_data->append_const(ir::Type::S, {constbits});
                _local_data.push_back(local_data);

                _buffer.append("lui", "a5", "%hi(" + local_data_name + ")");
                _buffer.append("flw", reg_str,
                               "%lo(" + local_data_name + ")(a5)");
            }
        } else {
            _buffer.append("li", reg_str, constbits->get_asm_value());
        }

        return reg_str;
    } else if (auto addr = std::dynamic_pointer_cast<ir::Address>(arg)) {
        int reg = get_temp_reg(addr->get_type(), no);
        std::string reg_str = regno2string(reg);

        _buffer.append("la", reg_str, addr->get_asm_value());

        return reg_str;
    } else {
        throw std::logic_error("unsupported type");
    }
}

std::tuple<std::string, bool>
Generator::_get_asm_arg_or_w_constbits(ir::ValuePtr arg, int no) {
    if (auto constbits = std::dynamic_pointer_cast<ir::ConstBits>(arg)) {
        if (constbits->get_type() == ir::Type::W) {
            return std::make_tuple(constbits->get_asm_value(), true);
        }
    }
    return std::make_tuple(_get_asm_arg(arg, no), false);
}

std::string Generator::_get_asm_addr(ir::ValuePtr arg, int no) {
    if (auto temp = std::dynamic_pointer_cast<ir::Temp>(arg)) {
        if (temp->reg >= 0) {
            return "0(" + regno2string(temp->reg) + ")";
        }

        if (temp->reg == SPILL) {
            int reg = _get_temp_reg(temp->get_type(), no);
            int offset = _stack_manager.get_spilled_temps_offset().at(temp);
            std::string reg_str = regno2string(reg);

            if (is_in_imm12_range(offset)) {
                _buffer.append("ld", reg_str, std::to_string(offset) + "(sp)");
            } else {
                _buffer.append("li", "a5", std::to_string(offset));
                _buffer.append("add", "a5", "sp", "a5");
                _buffer.append("ld", reg_str, "0(a5)");
            }

            return "0(" + reg_str + ")";
        } else if (temp->reg == STACK) {
            int offset;
            if (auto def_inst = std::get<ir::InstDef>(temp->defs[0]).ins;
                def_inst->insttype == ir::InstType::IADD) {
                auto alloc_temp =
                    std::static_pointer_cast<ir::Temp>(def_inst->arg[0]);
                auto add_const = std::get<int>(
                    std::static_pointer_cast<ir::ConstBits>(def_inst->arg[1])
                        ->value);
                offset = _stack_manager.get_local_var_offset().at(alloc_temp) +
                         add_const;
            } else {
                offset = _stack_manager.get_local_var_offset().at(temp);
            }
            if (is_in_imm12_range(offset)) {
                return std::to_string(offset) + "(sp)";
            } else {
                int reg = _get_temp_reg(temp->get_type(), no);
                std::string reg_str = regno2string(reg);
                _buffer.append("li", reg_str, std::to_string(offset));
                _buffer.append("add", reg_str, "sp", reg_str);
                return "0(" + reg_str + ")";
            }
        } else {
            throw std::logic_error("no register");
        }
    } else if (auto addr = std::dynamic_pointer_cast<ir::Address>(arg)) {
        auto name = addr->get_asm_value();
        _buffer.append("lui", "a5", "%hi(" + name + ")");
        return "%lo(" + name + ")(a5)";
    } else {
        throw std::logic_error("unsupported type");
    }
}

std::tuple<std::string, std::function<void(std::ostream &)>>
Generator::_get_asm_to(ir::TempPtr to,
                       std::function<int(ir::Type, int)> get_temp_reg) {
    if (to->reg >= 0) {
        return std::make_tuple(regno2string(to->reg),
                               [](std::ostream &out) { /* nop */ });
    }
    if (to->reg != SPILL) {
        throw std::logic_error("no register");
    }

    std::string store;
    switch (to->get_type()) {
    case ir::Type::W:
        store = "sw";
        break;
    case ir::Type::L:
        store = "sd";
        break;
    case ir::Type::S:
        store = "fsw";
        break;
    default:
        throw std::logic_error("unsupported type");
    }
    int offset = _stack_manager.get_spilled_temps_offset().at(to);
    int reg = get_temp_reg(to->get_type(), 0);
    std::string reg_str = regno2string(reg);

    return std::make_tuple(
        reg_str, [store, reg_str, offset, this](std::ostream &out) {
            if (is_in_imm12_range(offset)) {
                _buffer.append(store, reg_str, std::to_string(offset) + "(sp)");
            } else {
                _buffer.append("li", "a5", std::to_string(offset));
                _buffer.append("add", "a5", "sp", "a5");
                _buffer.append(store, reg_str, "0(a5)");
            }
        });
}

int Generator::_get_temp_reg(ir::Type type, int no) {
    std::array<int, 2> reg = {14, 15};            // a4, a5
    std::array<int, 2> freg = {32 + 14, 32 + 15}; // fa4, fa5
    if (type == ir::Type::S) {
        return freg[no];
    } else {
        return reg[no];
    }
}

} // namespace target
