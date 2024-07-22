#include "target/generator.h"
#include "array"
#include "target/mem.h"
#include "target/regalloc.h"
#include "target/utils.h"

namespace target {

#define INDENT "    "

// 12-bit immediate value range
static const int IMM12_MIN = -2048;
static const int IMM12_MAX = 2047;

static bool is_in_imm12_range(int value) {
    return value >= IMM12_MIN && value <= IMM12_MAX;
}

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

    _stack_manager = StackManager();
    _stack_manager.run(func);

    _out << ".text" << std::endl;

    if (func.is_export) {
        _out << ".global " << func.name << std::endl;
    }

    _out << func.name << ":" << std::endl;

    int frame_size = _stack_manager.get_frame_size();
    if (is_in_imm12_range(frame_size)) {
        _out << INDENT << build("addi", "sp", "sp", std::to_string(-frame_size))
             << std::endl;
    } else {
        _out << INDENT << build("li", "a5", std::to_string(frame_size))
             << std::endl;
        _out << INDENT << build("sub", "sp", "sp", "a5") << std::endl;
    }

    for (auto [reg, offset] : _stack_manager.get_callee_saved_regs_offset()) {
        if (is_in_imm12_range(offset)) {
            _out << INDENT
                 << build("sd", regno2string(reg),
                          std::to_string(offset) + "(sp)")
                 << std::endl;
        } else {
            _out << INDENT << build("li", "a5", std::to_string(offset))
                 << std::endl;
            _out << INDENT << build("add", "a5", "sp", "a5") << std::endl;
            _out << INDENT << build("sd", regno2string(reg), "0(a5)")
                 << std::endl;
        }
    }

    for (auto block = func.start; block; block = block->next) {
        _reg_reach = decltype(_reg_reach)(
            [](const RegReach &a, const RegReach &b) { return a.end < b.end; });

        _out << ".L" << block->id << ":" << std::endl;

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
        _generate_copy_like_inst(inst);
        break;
    case ir::InstType::IEXTSW:
    case ir::InstType::ISTOSI:
    case ir::InstType::ISWTOF:
        _generate_convert_inst(inst);
        break;
    case ir::InstType::ICEQZ: {
        auto [to, write_back] = _get_asm_to(inst.to);
        auto arg0 = _get_asm_arg(inst.arg[0], 0);

        _out << INDENT << build("seqz", to, arg0) << std::endl;

        write_back(_out);
    } break;
    case ir::InstType::IALLOC4:
    case ir::InstType::IALLOC8:
    case ir::InstType::INOP:
    case ir::InstType::ICEQW:
    case ir::InstType::ICNEW:
    case ir::InstType::ICSLEW:
    case ir::InstType::ICSLTW:
    case ir::InstType::ICSGEW:
    case ir::InstType::ICSGTW:
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

    _out << INDENT << build(inst2asm.at(inst.insttype), to, arg) << std::endl;

    write_back(_out);
}

void Generator::_generate_store_inst(const ir::Inst &inst) {
    static std::unordered_map<ir::InstType, std::string> inst2asm = {
        {ir::InstType::ISTOREW, "sw"},
        {ir::InstType::ISTOREL, "sd"},
        {ir::InstType::ISTORES, "fsw"},
    };

    auto arg0 = _get_asm_arg(inst.arg[0], 0);
    auto arg1 = _get_asm_addr(inst.arg[1], 1);

    _out << INDENT << build(inst2asm.at(inst.insttype), arg0, arg1)
         << std::endl;
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

    auto arg0 = _get_asm_arg(inst.arg[0], 0);
    auto arg1 = _get_asm_arg(inst.arg[1], 1);
    auto [to, write_back] = _get_asm_to(inst.to);

    auto inst_str = inst2asm.at(inst.insttype).at(inst.to->get_type());
    _out << INDENT << build(inst_str, to, arg0, arg1) << std::endl;
    write_back(_out);
}

void Generator::_generate_float_compare_inst(const ir::Inst &inst) {
    static std::unordered_map<ir::InstType, std::string> inst2asm = {
        {ir::InstType::ICEQS, "feq.s"}, {ir::InstType::ICNES, "fne.s"},
        {ir::InstType::ICLES, "fle.s"}, {ir::InstType::ICLTS, "flt.s"},
        {ir::InstType::ICGES, "fge.s"}, {ir::InstType::ICGTS, "fgt.s"},
    };

    auto [to, write_back] = _get_asm_to(inst.to);
    auto arg0 = _get_asm_arg(inst.arg[0], 0);
    auto arg1 = _get_asm_arg(inst.arg[1], 1);

    _out << INDENT << build(inst2asm.at(inst.insttype), to, arg0, arg1)
         << std::endl;

    write_back(_out);
}

void Generator::_generate_copy_like_inst(const ir::Inst &inst) {
    // clang-format off
    static std::unordered_map<ir::InstType, std::unordered_map<ir::Type, std::string>> inst2asm = {
        {ir::InstType::ICOPY, {{ir::Type::L, "mv"}, {ir::Type::W, "mv"}, {ir::Type::S, "fmv.s"}}},
        {ir::InstType::INEG, {{ir::Type::L, "neg"}, {ir::Type::W, "negw"}, {ir::Type::S, "fneg.s"}}},
    };
    // clang-format on

    auto [to, write_back] = _get_asm_to(inst.to);
    auto arg = _get_asm_arg(inst.arg[0], 0);

    auto inst_str = inst2asm.at(inst.insttype).at(inst.to->get_type());

    _out << INDENT << build(inst_str, to, arg) << std::endl;

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

    _out << INDENT << build(inst2asm.at(inst.insttype), to, arg);
    if (inst.insttype == ir::InstType::ISTOSI) {
        _out << ", rtz";
    }
    _out << std::endl;

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
            _out << INDENT
                 << build(store, regno2string(reg),
                          std::to_string(offset) + "(sp)")
                 << std::endl;
        } else {
            _out << INDENT << build("li", "a5", std::to_string(offset))
                 << std::endl;
            _out << INDENT << build("add", "a5", "sp", "a5") << std::endl;
            _out << INDENT << build(store, regno2string(reg), "0(a5)")
                 << std::endl;
        }
    }

    int arg_count = args.size() - 1;
    for (auto it = args.rbegin(); it != args.rend(); it++, arg_count--) {
        auto arg = *it;
        if (arg_count <= 7) {
            auto arg0 = _get_asm_arg(arg, 0);
            switch (arg->get_type()) {
            case ir::Type::W:
            case ir::Type::L: {
                _out << INDENT
                     << build("mv", "a" + std::to_string(arg_count), arg0)
                     << std::endl;
            } break;
            case ir::Type::S: {
                _out << INDENT
                     << build("fmv.s", "fa" + std::to_string(arg_count), arg0)
                     << std::endl;
            } break;
            default:
                throw std::logic_error("unsupported type");
            }
        } else {
            static std::unordered_map<ir::Type, std::string> inst2asm = {
                {ir::Type::W, "sw"}, {ir::Type::L, "sd"}, {ir::Type::S, "fsw"}};
            int offset = (arg_count - 8) * 8;
            auto arg0 = _get_asm_arg(arg, 0);
            if (is_in_imm12_range(offset)) {
                _out << INDENT
                     << build(inst2asm.at(arg->get_type()), arg0,
                              std::to_string(offset) + "(sp)")
                     << std::endl;
            } else {
                _out << INDENT << build("li", "a5", std::to_string(offset))
                     << std::endl;
                _out << INDENT << build("add", "a5", "sp", "a5") << std::endl;
                _out << INDENT
                     << build(inst2asm.at(arg->get_type()), arg0, "0(a5)")
                     << std::endl;
            }
        }
    }
    _out << INDENT
         << build("call",
                  std::static_pointer_cast<ir::Address>(inst.arg[0])->name)
         << std::endl;

    // restore caller saved registers
    for (auto [reg, offset] : _stack_manager.get_caller_saved_regs_offset()) {
        auto load = reg >= 32 ? "fld" : "ld";
        if (is_in_imm12_range(offset)) {
            _out << INDENT
                 << build(load, regno2string(reg),
                          std::to_string(offset) + "(sp)")
                 << std::endl;
        } else {
            _out << INDENT << build("li", "a5", std::to_string(offset))
                 << std::endl;
            _out << INDENT << build("add", "a5", "sp", "a5") << std::endl;
            _out << INDENT << build(load, regno2string(reg), "0(a5)")
                 << std::endl;
        }
    }

    if (inst.to != nullptr) {
        auto [to, write_back] = _get_asm_to(inst.to);
        switch (inst.to->get_type()) {
        case ir::Type::W:
        case ir::Type::L: {
            _out << INDENT << build("mv", to, "a0") << std::endl;
        } break;
        case ir::Type::S: {
            _out << INDENT << build("fmv.s", to, "fa0") << std::endl;
        } break;
        default:
            throw std::logic_error("unsupported type");
        }
        write_back(_out);
    }
}

void Generator::_generate_par_inst(const ir::Inst &inst, int par_count) {
    auto [to, write_back] = _get_asm_to(inst.to);
    if (par_count <= 7) { // in a0-a7
        switch (inst.to->get_type()) {
        case ir::Type::L:
        case ir::Type::W:
            _out << INDENT << build("mv", to, "a" + std::to_string(par_count))
                 << std::endl;
            break;
        case ir::Type::S:
            _out << INDENT
                 << build("fmv.s", to, "fa" + std::to_string(par_count))
                 << std::endl;
            break;
        default:
            break; // unreachable
        }
    } else {
        static std::unordered_map<ir::Type, std::string> inst2asm = {
            {ir::Type::W, "lw"}, {ir::Type::L, "ld"}, {ir::Type::S, "flw"}};
        int offset = (par_count - 8) * 8 + _stack_manager.get_frame_size();
        if (is_in_imm12_range(offset)) {
            _out << INDENT
                 << build(inst2asm.at(inst.to->get_type()), to,
                          std::to_string(offset) + "(sp)")
                 << std::endl;
        } else {
            _out << INDENT << build("li", "a5", std::to_string(offset))
                 << std::endl;
            _out << INDENT << build("add", "a5", "sp", "a5") << std::endl;
            _out << INDENT
                 << build(inst2asm.at(inst.to->get_type()), to, "0(a5)")
                 << std::endl;
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
                _out << INDENT << build("fmv.s", "fa0", arg) << std::endl;
            } else {
                _out << INDENT << build("mv", "a0", arg) << std::endl;
            }
        }

        // recover saved registers
        for (auto [reg, offset] :
             _stack_manager.get_callee_saved_regs_offset()) {

            if (is_in_imm12_range(offset)) {
                _out << INDENT
                     << build("ld", regno2string(reg),
                              std::to_string(offset) + "(sp)")
                     << std::endl;
            } else {
                _out << INDENT << build("li", "a5", std::to_string(offset))
                     << std::endl;
                _out << INDENT << build("add", "a5", "sp", "a5") << std::endl;
                _out << INDENT << build("ld", regno2string(reg), "0(a5)")
                     << std::endl;
            }
        }

        auto frame_size = _stack_manager.get_frame_size();
        if (is_in_imm12_range(frame_size)) {
            _out << INDENT
                 << build("addi", "sp", "sp",
                          std::to_string(_stack_manager.get_frame_size()))
                 << std::endl;
        } else {
            _out << INDENT << build("li", "a5", std::to_string(frame_size))
                 << std::endl;
            _out << INDENT << build("add", "sp", "sp", "a5") << std::endl;
        }

        _out << INDENT << build("jr", "ra") << std::endl;
    } break;
    case ir::Jump::JMP: {
        _out << INDENT << build("j", ".L" + std::to_string(jump.blk[0]->id))
             << std::endl;
    } break;
    case ir::Jump::JNZ: {
        _generate_jnz_inst(jump);
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

void Generator::_generate_jnz_inst(const ir::Jump &jump) {
    if (auto temp = std::dynamic_pointer_cast<ir::Temp>(jump.arg)) {
        if (temp->defs.size() != 1) {
            throw std::logic_error("requires single def");
        }
        if (auto instdef = std::get_if<ir::InstDef>(&temp->defs[0]);
            instdef != nullptr && is_int_compare(instdef->ins->insttype)) {
            auto arg0 = _get_asm_arg(instdef->ins->arg[0], 0);
            auto arg1 = _get_asm_arg(instdef->ins->arg[1], 1);
            switch (instdef->ins->insttype) {
            case ir::InstType::ICEQW:
                _out << INDENT
                     << build("beq", arg0, arg1,
                              ".L" + std::to_string(jump.blk[0]->id))
                     << std::endl;
                break;
            case ir::InstType::ICNEW:
                _out << INDENT
                     << build("bne", arg0, arg1,
                              ".L" + std::to_string(jump.blk[0]->id))
                     << std::endl;
                break;
            case ir::InstType::ICSLEW:
                _out << INDENT
                     << build("ble", arg0, arg1,
                              ".L" + std::to_string(jump.blk[0]->id))
                     << std::endl;
                break;
            case ir::InstType::ICSLTW:
                _out << INDENT
                     << build("blt", arg0, arg1,
                              ".L" + std::to_string(jump.blk[0]->id))
                     << std::endl;
                break;
            case ir::InstType::ICSGEW:
                _out << INDENT
                     << build("bge", arg0, arg1,
                              ".L" + std::to_string(jump.blk[0]->id))
                     << std::endl;
                break;
            case ir::InstType::ICSGTW:
                _out << INDENT
                     << build("bgt", arg0, arg1,
                              ".L" + std::to_string(jump.blk[0]->id))
                     << std::endl;
                break;
            default:
                throw std::logic_error("unsupported type");
            }
            _out << INDENT << build("j", ".L" + std::to_string(jump.blk[1]->id))
                 << std::endl;
            return;
        }
    }
    auto arg = _get_asm_arg(jump.arg, 0);
    _out << INDENT << build("bnez", arg, ".L" + std::to_string(jump.blk[0]->id))
         << std::endl;
    _out << INDENT << build("j", ".L" + std::to_string(jump.blk[1]->id))
         << std::endl;
}

std::string Generator::_get_asm_arg(ir::ValuePtr arg, int no) {
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
                throw "flw";
            default:
                throw std::logic_error("unsupported type");
            };
            int offset = _stack_manager.get_spilled_temps_offset().at(temp);
            int reg = _get_temp_reg(temp->get_type(), no);
            std::string reg_str = regno2string(reg);

            if (is_in_imm12_range(offset)) {
                _out << INDENT
                     << build(load, reg_str, std::to_string(offset) + "(sp)")
                     << std::endl;
            } else {
                _out << INDENT << build("li", "a5", std::to_string(offset))
                     << std::endl;
                _out << INDENT << build("add", "a5", "sp", "a5") << std::endl;
                _out << INDENT << build(load, reg_str, "0(a5)") << std::endl;
            }

            return reg_str;
        } else if (temp->reg == STACK) {
            int offset = _stack_manager.get_local_var_offset().at(temp);
            int reg = _get_temp_reg(temp->get_type(), no);
            std::string reg_str = regno2string(reg);

            if (is_in_imm12_range(offset)) {
                _out << INDENT
                     << build("addi", reg_str, "sp", std::to_string(offset))
                     << std::endl;
            } else {
                _out << INDENT << build("li", reg_str, std::to_string(offset))
                     << std::endl;
                _out << INDENT << build("add", reg_str, "sp", reg_str)
                     << std::endl;
            }

            return reg_str;
        } else {
            throw std::logic_error("no register");
        }
    } else if (auto constbits = std::dynamic_pointer_cast<ir::ConstBits>(arg)) {
        int reg = _get_temp_reg(constbits->get_type(), no);
        std::string reg_str = regno2string(reg);

        if (constbits->get_type() == ir::Type::S) {
            std::string local_data_name =
                ".LC" + std::to_string(_local_data.size());

            auto local_data = std::shared_ptr<ir::Data>(
                new ir::Data{false, local_data_name, 4});
            local_data->append_const(ir::Type::S, {constbits});
            _local_data.push_back(local_data);

            _out << INDENT << build("lui", "a5", "%hi(" + local_data_name + ")")
                 << std::endl;
            _out << INDENT
                 << build("flw", reg_str, "%lo(" + local_data_name + ")(a5)")
                 << std::endl;
        } else {
            _out << INDENT << build("li", reg_str, constbits->get_asm_value())
                 << std::endl;
        }

        return reg_str;
    } else if (auto addr = std::dynamic_pointer_cast<ir::Address>(arg)) {
        int reg = _get_temp_reg(addr->get_type(), no);
        std::string reg_str = regno2string(reg);

        _out << INDENT << build("la", reg_str, addr->get_asm_value())
             << std::endl;

        return reg_str;
    } else {
        throw std::logic_error("unsupported type");
    }
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
                _out << INDENT
                     << build("ld", reg_str, std::to_string(offset) + "(sp)")
                     << std::endl;
            } else {
                _out << INDENT << build("li", "a5", std::to_string(offset))
                     << std::endl;
                _out << INDENT << build("add", "a5", "sp", "a5") << std::endl;
                _out << INDENT << build("ld", reg_str, "0(a5)") << std::endl;
            }

            return "0(" + reg_str + ")";
        } else if (temp->reg == STACK) {
            int offset = _stack_manager.get_local_var_offset().at(temp);
            if (is_in_imm12_range(offset)) {
                return std::to_string(offset) + "(sp)";
            } else {
                int reg = _get_temp_reg(temp->get_type(), no);
                std::string reg_str = regno2string(reg);
                _out << INDENT << build("li", reg_str, std::to_string(offset))
                     << std::endl;
                _out << INDENT << build("add", reg_str, "sp", reg_str)
                     << std::endl;
                return "0(" + reg_str + ")";
            }
        } else {
            throw std::logic_error("no register");
        }
    } else if (auto addr = std::dynamic_pointer_cast<ir::Address>(arg)) {
        auto name = addr->get_asm_value();
        _out << INDENT << build("lui", "a5", "%hi(" + name + ")") << std::endl;
        return "%lo(" + name + ")(a5)";
        return addr->get_asm_value();
    } else {
        throw std::logic_error("unsupported type");
    }
}

std::tuple<std::string, std::function<void(std::ostream &)>>
Generator::_get_asm_to(ir::TempPtr to) {
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
    int reg = _get_temp_reg(to->get_type(), 0);
    std::string reg_str = regno2string(reg);

    return std::make_tuple(
        reg_str, [store, reg_str, offset](std::ostream &out) {
            if (is_in_imm12_range(offset)) {
                out << INDENT
                    << build(store, reg_str, std::to_string(offset) + "(sp)")
                    << std::endl;
            } else {
                out << INDENT << build("li", "a5", std::to_string(offset))
                    << std::endl;
                out << INDENT << build("add", "a5", "sp", "a5") << std::endl;
                out << INDENT << build(store, reg_str, "0(a5)") << std::endl;
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
