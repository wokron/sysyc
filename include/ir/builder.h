#pragma once

#include "ir.h"

namespace ir {

using ValuePtr = std::shared_ptr<Value>;

class IRBuilder {
  private:
    std::shared_ptr<Function> _function;
    std::shared_ptr<Block> _insert_point = nullptr;

  public:
    IRBuilder(std::shared_ptr<Function> function) : _function(function) {}

    void set_insert_point(std::shared_ptr<Block> block) {
        _insert_point = block;
    }

    ValuePtr insert_inst(std::shared_ptr<Inst> inst) {
        auto insert_point = _insert_point;
        if (!insert_point) {
            insert_point = _function->end;
        }

        insert_point->insts.push_back(inst);
        if (inst->to) {
            inst->to->id = _function->temp_counter++;
        }
        return inst->to;
    }

    std::shared_ptr<Block> create_label(std::string name) {
        auto block = Block::create(name, *_function);
        return block;
    }

    ValuePtr create_add(Type ty, ValuePtr lhs, ValuePtr rhs);
    ValuePtr create_sub(Type ty, ValuePtr lhs, ValuePtr rhs);
    ValuePtr create_neg(Type ty, ValuePtr operand);
    ValuePtr create_div(Type ty, ValuePtr lhs, ValuePtr rhs);
    ValuePtr create_mul(Type ty, ValuePtr lhs, ValuePtr rhs);
    ValuePtr create_rem(Type ty, ValuePtr lhs, ValuePtr rhs);

    void create_store(Type ty, ValuePtr address, ValuePtr value);
    ValuePtr create_load(Type ty, ValuePtr address);
    ValuePtr create_alloc(Type ty, int bytes);

    ValuePtr create_ceqw(ValuePtr lhs, ValuePtr rhs);
    ValuePtr create_cnew(ValuePtr lhs, ValuePtr rhs);
    ValuePtr create_cslew(ValuePtr lhs, ValuePtr rhs);
    ValuePtr create_csltw(ValuePtr lhs, ValuePtr rhs);
    ValuePtr create_csgew(ValuePtr lhs, ValuePtr rhs);
    ValuePtr create_csgtw(ValuePtr lhs, ValuePtr rhs);

    ValuePtr create_ceqs(ValuePtr lhs, ValuePtr rhs);
    ValuePtr create_cnes(ValuePtr lhs, ValuePtr rhs);
    ValuePtr create_cles(ValuePtr lhs, ValuePtr rhs);
    ValuePtr create_clts(ValuePtr lhs, ValuePtr rhs);
    ValuePtr create_cges(ValuePtr lhs, ValuePtr rhs);
    ValuePtr create_cgts(ValuePtr lhs, ValuePtr rhs);

    ValuePtr create_stosi(ValuePtr value);
    ValuePtr create_swtof(Type ty, ValuePtr value);

    ValuePtr create_call(Type ty, ValuePtr func, std::vector<ValuePtr> args);

    void create_ret(ValuePtr value);
    void create_jmp(std::shared_ptr<Block> target);
    void create_jnz(ValuePtr cond, std::shared_ptr<Block> iftrue,
                    std::shared_ptr<Block> iffalse);
};

} // namespace ir
