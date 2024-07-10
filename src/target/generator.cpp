#include "target/generator.h"
#include "target/mem.h"
#include "target/utils.h"

#define INDENT "    "

namespace target {

void Generator::generate(const ir::Module &module) {
    for (const auto &data : module.datas) {
        generate_data(*data);
    }

    for (const auto &func : module.functions) {
        generate_func(*func);
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
    StackManager stack_manager;
    stack_manager.run(func);

    _out << ".text" << std::endl;

    if (func.is_export) {
        _out << ".global " << func.name << std::endl;
    }

    _out << func.name << ":" << std::endl;

    _out << INDENT
         << build("addi", "sp", "sp",
                  "-" + std::to_string(stack_manager.get_frame_size()))
         << std::endl;
    for (auto [reg, offset] : stack_manager.get_callee_saved_regs_offset()) {
        _out << INDENT
             << build("sd", regno2string(reg), std::to_string(offset) + "(sp)")
             << std::endl;
    }

    for (auto block = func.start; block; block = block->next) {
        _out << ".L" << block->id << ":" << std::endl;

        std::vector<ir::ValuePtr> call_params;
        for (const auto &inst : block->insts) {
            if (inst->insttype == ir::InstType::IPAR) {
                call_params.push_back(inst->arg[0]);
            } else if (inst->insttype == ir::InstType::ICALL) {
                _generate_call_inst(*inst, call_params);
                call_params.clear();
            } else {
                _generate_inst(*inst, stack_manager);
            }
        }
        _generate_jump_inst(block->jump, stack_manager);
    }

    _out << ".type " << func.name << ", @function" << std::endl;
    _out << ".size " << func.name << ", .-" << func.name << std::endl;
    _out << "/* end function " << func.name << " */" << std::endl << std::endl;
}

void Generator::_generate_inst(const ir::Inst &inst, StackManager &stack_manager) {
    switch (inst.insttype) {
    case ir::InstType::IALLOC4:
    case ir::InstType::IALLOC8:
        _generate_alloc_inst(inst, stack_manager);
        break;
    case ir::InstType::ISTOREW:
    case ir::InstType::ISTOREL:
    case ir::InstType::ISTORES:
        _generate_store_inst(inst, stack_manager);
        break;
    case ir::InstType::ILOADW:
    case ir::InstType::ILOADL:
    case ir::InstType::ILOADS:
        _generate_load_inst(inst, stack_manager);
        break;
    case ir::InstType::IADD:
        _generate_add_inst(inst, stack_manager);
        break;
    default:
        _out << INDENT << "nop" << std::endl;
    }
}

void Generator::_generate_call_inst(const ir::Inst &call_inst,
                                    std::vector<ir::ValuePtr> params) {}

std::string get_const_asm_value(ir::ValuePtr value) {
    if (auto const_value = std::dynamic_pointer_cast<ir::Const>(value)) {
        return const_value->get_asm_value();
    } else {
        throw std::logic_error("unsupported value type");
    }
}

std::string get_asm_to(ir::ValuePtr value) {
    if (auto temp_value = std::dynamic_pointer_cast<ir::Temp>(value)) {
        if (temp_value->reg < 0) {
            temp_value->reg = get_temp_reg();
        }
        return regno2string(temp_value->reg);
    } else {
        throw std::logic_error("unsupported value type");
    }
}

std::string get_asm_arg(ir::ValuePtr value, StackManager &stack_manager, std::ostream &out, bool support_immediate = true) {
    if (auto temp_value = std::dynamic_pointer_cast<ir::Temp>(value)) {
        if (temp_value->reg >= 0) {
            return regno2string(temp_value->reg);
        }
            
        int offset;
        temp_value->reg = get_temp_reg();
        if (temp_value->reg == -1) {
            offset = stack_manager.get_spilled_temps_offset().at(temp_value);
        } else if (temp_value->reg == -2) {
            offset = stack_manager.get_local_var_offset().at(temp_value);
        } else {
            throw std::logic_error("no register");
        }
        std::string lw;
        switch (temp_value->get_type()) {
        case ir::Type::W:
            lw = "lw";
            break;
        case ir::Type::L:
            lw = "ld";
            break;
        case ir::Type::S:
            throw std::logic_error("not implemented");
        default:
            throw std::logic_error("unsupported type");
        };
        out << INDENT
            << build(lw, regno2string(temp_value->reg), std::to_string(offset) + "(sp)")
            << std::endl;
            
        return regno2string(temp_value->reg);
    } else if (auto const_value = std::dynamic_pointer_cast<ir::Const>(value)) {
        if (support_immediate) {
            return get_const_asm_value(value);
        } else {
            const_value->get_type();
            int reg = get_temp_reg();
            out << INDENT
                << build("li", regno2string(reg), get_const_asm_value(value))
                << std::endl;
            return regno2string(reg);
        }
    } else {
        throw std::logic_error("unsupported value type");
    }
}

std::string get_asm_addr(ir::ValuePtr value, StackManager &stack_manager) {
    if (auto temp_value = std::dynamic_pointer_cast<ir::Temp>(value)) {
        if (temp_value->reg >= 0) {
            return regno2string(temp_value->reg);
        }

        int offset;
        if (temp_value->reg == -1) {
            offset = stack_manager.get_spilled_temps_offset().at(temp_value);
        } else if (temp_value->reg == -2) {
            offset = stack_manager.get_local_var_offset().at(temp_value);
        } else {
            throw std::logic_error("no register");
        }

        return std::to_string(offset) + "(sp)";
    } else {
        throw std::logic_error("unsupported value type");
    }
}

void Generator::_generate_alloc_inst(const ir::Inst &inst, StackManager &stack_manager) {
    if (inst.to->reg == -3) {
        inst.to->reg = -2;
    }
}

void Generator::_generate_store_inst(const ir::Inst &inst, StackManager &stack_manager) {
    std::string store;
    switch (inst.insttype)
    {
    case ir::InstType::ISTOREW:
        store = "sw";
        break;
    case ir::InstType::ISTOREL:
        store = "sd";
        break;
    case ir::InstType::ISTORES:
        throw std::logic_error("not implemented");
    default:
        break; // unreachable
    }
    
    std::string arg = get_asm_arg(inst.arg[0], stack_manager, _out, false);
    std::string addr = get_asm_addr(inst.arg[1], stack_manager);

    _out << INDENT
         << build(store, arg, addr)
         << std::endl;
}

void Generator::_generate_load_inst(const ir::Inst &inst, StackManager &stack_manager) {
    std::string lw;
    switch (inst.insttype) {
    case ir::InstType::ILOADW:
        lw = "lw";
        break;
    case ir::InstType::ILOADL:
        lw = "ld";
        break;
    case ir::InstType::ILOADS:
        throw std::logic_error("not implemented");
    default:
        break; // unreachable
    }

    auto to = get_asm_to(inst.to);
    auto addr = get_asm_addr(inst.arg[0], stack_manager);

    _out << INDENT
         << build(lw, to, addr)
         << std::endl;
}

void Generator::_generate_add_inst(const ir::Inst &inst, StackManager& stack_manager) {
    std::string add;
    std::string addi;
    auto temp_arg0 = std::dynamic_pointer_cast<ir::Temp>(inst.arg[0]);
    auto temp_arg1 = std::dynamic_pointer_cast<ir::Temp>(inst.arg[1]);

    auto to = get_asm_to(inst.to);
    auto arg0 = get_asm_arg(inst.arg[0], stack_manager, _out);
    auto arg1 = get_asm_arg(inst.arg[1], stack_manager, _out);

    switch (inst.to->get_type()) {
    case ir::Type::W:
    case ir::Type::L: {
        std::string add = inst.to->get_type() == ir::Type::W ? "addw" : "add";
        std::string addi =
            inst.to->get_type() == ir::Type::W ? "addiw" : "addi";
        if (temp_arg0 && temp_arg1) {
            _out << INDENT
                 << build(add, to, arg0, arg1)
                 << std::endl;
        } else if (temp_arg0) {
            _out << INDENT
                 << build(addi,to, arg0, arg1)
                 << std::endl;
        } else if (temp_arg1) {
            _out << INDENT
                 << build(addi, to, arg1, arg0)
                 << std::endl;
        } else {
            _out << INDENT
                 << build("li", to, get_const_asm_value(inst.arg[0]))
                 << std::endl;
            _out << INDENT
                 << build(addi, to, to, get_const_asm_value(inst.arg[1]))
                 << std::endl;
        }
    } break;
    case ir::Type::S: {
        std::string reg0, reg1;
        if (temp_arg0) {
            reg0 = regno2string(temp_arg0->reg);
        } else {

            _out << INDENT
                 << build("li", "a0", get_const_asm_value(inst.arg[0]))
                 << std::endl;
            reg0 = "a0";
        }
    } break;
    default:
        throw std::logic_error("unsupported type");
    }
}

void Generator::_generate_jump_inst(const ir::Jump &jump, StackManager &stack_manager) {
    switch (jump.type)
    {
    case ir::Jump::NONE:
        return;
    case ir::Jump::RET: {
        if (jump.arg) {
            auto arg = get_asm_arg(jump.arg, stack_manager, _out, false);
            _out << INDENT
                 << build("mv", "a0", arg)
                 << std::endl;
        }
        _out << INDENT
            << build("jr", "ra")
            << std::endl;
    } break;
    case ir::Jump::JMP: {
        _out << INDENT
             << build("j", ".L" + std::to_string(jump.blk[0]->id))
             << std::endl;
    } break;
    default:
        throw std::logic_error("unsupported jump type");
    }
}

} // namespace target
