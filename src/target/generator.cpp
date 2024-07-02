#include "target/generator.h"

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
    _out << ".text" << std::endl;

    if (func.is_export) {
        _out << ".global " << func.name << std::endl;
    }

    _out << func.name << ":" << std::endl;

    for (auto block = func.start; block; block = block->next) {
        _out << ".L" << block->id << ":" << std::endl;

        for (const auto &inst : block->insts) {
            _out << INDENT;

            // TODO: generate instructions
            _out << "nop";

            _out << std::endl;
        }
    }

    _out << ".type " << func.name << ", @function" << std::endl;
    _out << ".size " << func.name << ", .-" << func.name << std::endl;
    _out << "/* end function " << func.name << " */" << std::endl << std::endl;
}

} // namespace target
