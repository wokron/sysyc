#pragma once

#include "folder.h"
#include "ir.h"
#include <cassert>

namespace ir {

using ValuePtr = std::shared_ptr<Value>;

class IRBuilder {
private:
    std::shared_ptr<Function> _function = nullptr;
    std::shared_ptr<Block> _insert_point = nullptr;
    Folder _folder;

    // require constant means that the result of the instruction must be a
    // constant, in other words, instruction cannot be inserted.
    bool _require_constant = false;

public:
    IRBuilder() = default;
    IRBuilder(std::shared_ptr<Function> function) : _function(function) {}

    void set_function(std::shared_ptr<Function> function) {
        _function = function;
        _insert_point = nullptr;
    }

    void set_insert_point(std::shared_ptr<Block> block) {
        _insert_point = block;
    }

    void set_require_constant(bool require_constant) {
        _require_constant = require_constant;
    }

    ValuePtr insert_inst(std::shared_ptr<Inst> inst) {
        if (_require_constant) {
            // shouldn't insert instruction if constant is required
            return nullptr;
        }

        // if constant is not required, inst should be inserted into a block
        auto insert_point = get_insert_point();

        insert_point->insts.push_back(inst);
        if (inst->to) {
            inst->to->id = _function->temp_counter++;
        }
        return inst->to;
    }

    std::shared_ptr<Block> get_insert_point() {
        if (_insert_point) {
            return _insert_point;
        }
        assert(_function);
        return _function->end;
    }

    std::shared_ptr<Block> create_label(std::string name) {
        auto prev = _function->end;
        auto block = Block::create(name, *_function);
        if (prev && prev->jump.type == Jump::NONE) {
            prev->jump = {
                .type = Jump::JMP,
                .blk = {block},
            };
        }
        return block;
    }

    ValuePtr create_add(Type ty, ValuePtr lhs, ValuePtr rhs);
    ValuePtr create_sub(Type ty, ValuePtr lhs, ValuePtr rhs);
    ValuePtr create_neg(Type ty, ValuePtr operand);
    ValuePtr create_div(Type ty, ValuePtr lhs, ValuePtr rhs);
    ValuePtr create_mul(Type ty, ValuePtr lhs, ValuePtr rhs);
    ValuePtr create_rem(Type ty, ValuePtr lhs, ValuePtr rhs);

    void create_store(Type ty, ValuePtr value, ValuePtr address);
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

    ValuePtr create_extsw(ValuePtr value);
    ValuePtr create_stosi(ValuePtr value);
    ValuePtr create_swtof(ValuePtr value);

    ValuePtr create_call(Type ty, ValuePtr func, std::vector<ValuePtr> args);

    std::shared_ptr<Block> create_ret(ValuePtr value);
    std::shared_ptr<Block> create_jmp(std::shared_ptr<Block> target);
    std::shared_ptr<Block> create_jnz(ValuePtr cond,
                                      std::shared_ptr<Block> iftrue,
                                      std::shared_ptr<Block> iffalse);
};

} // namespace ir
