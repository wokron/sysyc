#include "ir/ir.h"
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>

#define INDENT "    "

namespace ir {

std::unordered_map<InstType, std::string> inst2name = {
#define OP(op, name) {op, name},
#include "ir/ops.h"
#undef OP
};

std::unordered_map<float, std::shared_ptr<ConstBits>> ConstBits::floatcon_cache;
std::unordered_map<int, std::shared_ptr<ConstBits>> ConstBits::intcon_cache;

void ConstBits::emit(std::ostream &out) const {
    std::visit(overloaded{
                   [&out](int value) { out << value; },
                   [&out](float value) {
                       // we need to emit the float with full precision
                       // s_ prefix for single precision
                       out << std::setprecision(
                                  std::numeric_limits<float>::max_digits10)
                           << "s_" << value;
                   },
               },
               value);
}

Type ConstBits::get_type() const {
    if (auto value = std::get_if<float>(&this->value); value != nullptr) {
        return Type::S;
    } else if (auto value = std::get_if<int>(&this->value); value != nullptr) {
        return Type::W;
    } else {
        return Type::X;
    }
}

std::shared_ptr<Inst> Inst::create(InstType insttype, Type ty,
                                   std::shared_ptr<Value> arg0,
                                   std::shared_ptr<Value> arg1) {
    auto inst =
        std::shared_ptr<Inst>(new Inst{insttype, nullptr, {arg0, arg1}});
    if (ty != Type::X) {
        inst->to = std::make_shared<Temp>("", ty, std::vector<Def>{InstDef{inst}});
    }

    if (arg0)
        if (auto arg0 = std::dynamic_pointer_cast<Temp>(inst->arg[0]); arg0) {
            arg0->uses.push_back(Use(InstUse{inst}));
        }

    if (arg1)
        if (auto arg1 = std::dynamic_pointer_cast<Temp>(inst->arg[1]); arg1) {
            arg1->uses.push_back(Use(InstUse{inst}));
        }

    return inst;
}

void Inst::emit(std::ostream &out) const {

    if (to) {
        to->emit(out);
        out << " =" << type_to_string(to->type) << " ";
    }
    out << inst2name[insttype] << " ";
    if (arg[0]) {
        arg[0]->emit(out);
    }
    if (arg[1]) {
        out << ", ";
        arg[1]->emit(out);
    }
}

void Phi::emit(std::ostream &out) const {
    to->emit(out);
    out << " =" << type_to_string(to->type) << " phi ";
    bool first = true;
    for (auto &[block, value] : args) {
        if (first) {
            first = false;
        } else {
            out << ", ";
        }
        out << "@" << block->get_name() << " ";
        if (value == nullptr) {
            out << "0"; // hope this can compile
        } else {
            value->emit(out);
        }
    }
}

std::shared_ptr<Block> Block::create(std::string name, Function &func) {
    // use a counter to generate unique block id in file scope
    uint *block_counter_ptr = func.block_counter_ptr;
    if (func.start == nullptr) {
        func.start =
            std::shared_ptr<Block>(new Block{(*block_counter_ptr)++, name});
        func.end = func.start;
        return func.start;
    } else {
        auto blk =
            std::shared_ptr<Block>(new Block{(*block_counter_ptr)++, name});
        func.end->next = blk;
        func.end = blk;
        return blk;
    }
}

void Block::emit(std::ostream &out) const {
    out << "@" << get_name() << std::endl;

    for (auto &phi : phis) {
        out << INDENT;
        phi->emit(out);
        out << std::endl;
    }

    std::string params;
    for (auto &inst : insts) {
        if (inst->insttype == InstType::IPAR) {
            continue;
        } else if (inst->insttype == InstType::IARG) {
            std::ostringstream tempout;
            inst->arg[0]->emit(tempout);
            params += type_to_string(inst->arg[0]->get_type()) + " " +
                      tempout.str() + ", ";
            continue;
        }

        out << INDENT;
        inst->emit(out);
        if (inst->insttype == InstType::ICALL) {
            out << "(" << params << ")";
            params.clear();
        }
        out << std::endl;
    }

    switch (jump.type) {
    case Jump::JMP:
        out << INDENT "jmp @" << jump.blk[0]->get_name() << std::endl;
        break;
    case Jump::JNZ:
        out << INDENT "jnz ";
        jump.arg->emit(out);
        out << ", @" << jump.blk[0]->get_name() << ", @"
            << jump.blk[1]->get_name() << std::endl;
        break;
    case Jump::RET:
        out << INDENT "ret";
        if (jump.arg) {
            out << " ";
            jump.arg->emit(out);
        }
        out << std::endl;
        break;
    default: // Jump::NONE
        break;
    }
}

std::tuple<Function::FunctionPtr, Function::TempPtrList>
Function::create(bool is_export, std::string name, Type ty,
                 std::vector<Type> params, Module &module) {

    auto func = std::shared_ptr<Function>(new Function{
        .is_export = is_export,
        .name = name,
        .ty = ty,
        .block_counter_ptr = &module.block_counter,
    });

    auto start = Block::create("start", *func);

    std::vector<std::shared_ptr<Temp>> params_temps;
    for (auto &ty : params) {
        auto inst = Inst::create(InstType::IPAR, ty, nullptr, nullptr);
        start->insts.push_back(inst);
        inst->to->id = func->temp_counter++;
        params_temps.push_back(inst->to);
    }

    module.add_function(func);
    return std::make_tuple(func, params_temps);
}

void Function::emit(std::ostream &out) const {
    if (is_export) {
        out << "export" << std::endl;
    }
    out << "function" << (ty != Type::X ? " " + type_to_string(ty) : "") << " $"
        << name << "(";

    std::string params;
    for (auto &inst : start->insts) {
        if (inst->insttype == InstType::IPAR) {
            std::ostringstream tempout;
            inst->to->emit(tempout);
            params += type_to_string(inst->to->get_type()) + " " +
                      tempout.str() + ", ";
        }
    }
    out << params << ") {" << std::endl;

    for (auto blk = start; blk; blk = blk->next) {
        blk->emit(out);
    }
    out << "}" << std::endl;
}

void ConstData::emit(std::ostream &out) const {
    out << type_to_string(ty);
    for (auto &value : values) {
        out << " ";
        value->emit(out);
    }
}

std::shared_ptr<Data> Data::create(bool is_export, std::string name, int align,
                                   Module &module) {
    auto data = std::shared_ptr<Data>(new Data{is_export, name, align});
    module.add_data(data);
    return data;
}

Data &Data::append_const(Type ty, std::vector<std::shared_ptr<Const>> values) {
    items.push_back(std::make_unique<ConstData>(ty, values));
    return *this;
}

Data &Data::append_zero(int bytes) {
    items.push_back(std::make_unique<ZeroData>(bytes));
    return *this;
}

void Data::emit(std::ostream &out) const {
    if (is_export) {
        out << "export ";
    }
    out << "data $" << name << " = align " << align << " { ";

    for (auto &item : items) {
        item->emit(out);
        out << ", ";
    }

    out << "}" << std::endl;
}

void Module::emit(std::ostream &out) const {
    for (auto &data : datas) {
        data->emit(out);
    }

    for (auto &func : functions) {
        func->emit(out);
    }
}

std::unordered_map<std::string, std::shared_ptr<Address>>
    Address::addrcon_cache;

std::shared_ptr<Address> Address::get(std::string name) {
    if (auto it = addrcon_cache.find(name); it != addrcon_cache.end()) {
        return it->second;
    } else {
        return addrcon_cache[name] = std::make_shared<Address>(name);
    }
}

} // namespace ir
