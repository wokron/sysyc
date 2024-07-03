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
                _generate_inst(*inst);
            }
        }
    }

    _out << ".type " << func.name << ", @function" << std::endl;
    _out << ".size " << func.name << ", .-" << func.name << std::endl;
    _out << "/* end function " << func.name << " */" << std::endl << std::endl;
}

void Generator::_generate_inst(const ir::Inst &inst) {
    _out << INDENT << "nop" << std::endl;
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

// void Generator::_generate_add_inst(const ir::Inst &inst) {
//     std::string add;
//     std::string addi;
//     auto temp_arg0 = std::dynamic_pointer_cast<ir::Temp>(inst.arg[0]);
//     auto temp_arg1 = std::dynamic_pointer_cast<ir::Temp>(inst.arg[1]);

//     switch (inst.to->get_type()) {
//     case ir::Type::W:
//     case ir::Type::L: {
//         std::string add = inst.to->get_type() == ir::Type::W ? "addw" : "add";
//         std::string addi =
//             inst.to->get_type() == ir::Type::W ? "addiw" : "addi";
//         if (temp_arg0 && temp_arg1) {
//             _out << INDENT
//                  << build(add, regno2string(inst.to->reg),
//                           regno2string(temp_arg0->reg),
//                           regno2string(temp_arg1->reg))
//                  << std::endl;
//         } else if (temp_arg0) {
//             _out << INDENT
//                  << build(addi, regno2string(inst.to->reg),
//                           regno2string(temp_arg0->reg),
//                           get_const_asm_value(inst.arg[0]))
//                  << std::endl;
//         } else if (temp_arg1) {
//             _out << INDENT
//                  << build(addi, regno2string(inst.to->reg),
//                           regno2string(temp_arg1->reg),
//                           get_const_asm_value(inst.arg[0]))
//                  << std::endl;
//         } else {
//             _out << INDENT
//                  << build("li", regno2string(inst.to->reg),
//                           get_const_asm_value(inst.arg[0]))
//                  << std::endl;
//             _out << INDENT
//                  << build(addi, regno2string(inst.to->reg),
//                           regno2string(inst.to->reg),
//                           get_const_asm_value(inst.arg[1]))
//                  << std::endl;
//         }
//     } break;
//     case ir::Type::S: {
//         std::string reg0, reg1;
//         if (temp_arg0) {
//             reg0 = regno2string(temp_arg0->reg);
//         } else {

//             _out << INDENT
//                  << build("li", "a0", get_const_asm_value(inst.arg[0]))
//                  << std::endl;
//             reg0 = "a0";
//         }
//     } break;
//     default:
//         throw std::logic_error("unsupported type");
//     }
// }

} // namespace target
