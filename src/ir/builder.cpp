#include "ir/builder.h"

namespace ir {

/**
 * @brief Set the insert point to the given block
 * format: %to =<ty> add <lhs>, <rhs>
 */
ValuePtr IRBuilder::create_add(Type ty, ValuePtr lhs, ValuePtr rhs) {
    if (auto const_val = _folder.fold_add(lhs, rhs); const_val) {
        return const_val;
    }
    auto inst = Inst::create(InstType::IADD, ty, lhs, rhs);
    return insert_inst(inst);
}

/**
 * @brief Create a subtraction instruction,
 * format: %to =<ty> sub <lhs>, <rhs>
 */
ValuePtr IRBuilder::create_sub(Type ty, ValuePtr lhs, ValuePtr rhs) {
    if (auto const_val = _folder.fold_sub(lhs, rhs); const_val) {
        return const_val;
    }
    auto inst = Inst::create(InstType::ISUB, ty, lhs, rhs);
    return insert_inst(inst);
}

/**
 * @brief Create a negation instruction,
 * format: %to =<ty> neg <operand>
 */
ValuePtr IRBuilder::create_neg(Type ty, ValuePtr operand) {
    if (auto const_val = _folder.fold_neg(operand); const_val) {
        return const_val;
    }
    auto inst = Inst::create(InstType::INEG, ty, operand, nullptr);
    return insert_inst(inst);
}

/**
 * @brief Create a divide instruction,
 * format: %to =<ty> div <lhs>, <rhs>
 */
ValuePtr IRBuilder::create_div(Type ty, ValuePtr lhs, ValuePtr rhs) {
    if (auto const_val = _folder.fold_div(lhs, rhs); const_val) {
        return const_val;
    }
    auto inst = Inst::create(InstType::IDIV, ty, lhs, rhs);
    return insert_inst(inst);
}

/**
 * @brief Create a multiply instruction,
 * format: %to =<ty> mul <lhs>, <rhs>
 */
ValuePtr IRBuilder::create_mul(Type ty, ValuePtr lhs, ValuePtr rhs) {
    if (auto const_val = _folder.fold_mul(lhs, rhs); const_val) {
        return const_val;
    }
    auto inst = Inst::create(InstType::IMUL, ty, lhs, rhs);
    return insert_inst(inst);
}

/**
 * @brief Create a remainder instruction,
 * format: %to =<ty> rem <lhs>, <rhs>
 */
ValuePtr IRBuilder::create_rem(Type ty, ValuePtr lhs, ValuePtr rhs) {
    if (auto const_val = _folder.fold_rem(lhs, rhs); const_val) {
        return const_val;
    }
    auto inst = Inst::create(InstType::IREM, ty, lhs, rhs);
    return insert_inst(inst);
}

/**
 * @brief Create a store instruction,
 * format: store<ty> <value> <address>
 */
void IRBuilder::create_store(Type ty, ValuePtr value, ValuePtr address) {
    InstType op;
    switch (ty) {
    case Type::W:
        op = InstType::ISTOREW;
        break;
    case Type::S:
        op = InstType::ISTORES;
        break;
    case Type::L:
        op = InstType::ISTOREL;
        break;
    default:
        break; // unreachable
    }

    auto inst = Inst::create(op, Type::X, value, address);
    insert_inst(inst);
}

/**
 * @brief Create a load instruction,
 * format: %to =<ty> load<ty> <address>
 */
ValuePtr IRBuilder::create_load(Type ty, ValuePtr address) {
    InstType op;
    switch (ty) {
    case Type::W:
        op = InstType::ILOADW;
        break;
    case Type::S:
        op = InstType::ILOADS;
        break;
    case Type::L:
        op = InstType::ILOADL;
        break;
    default:
        break; // unreachable
    }

    auto inst = Inst::create(op, ty, address, nullptr);
    return insert_inst(inst);
}

/**
 * @brief Create an allocation instruction,
 * format: %to =l alloc<ty> <bytes>
 */
ValuePtr IRBuilder::create_alloc(Type ty, int bytes) {
    InstType op;
    switch (ty) {
    case Type::W:
    case Type::S:
        op = InstType::IALLOC4;
        break;
    case Type::L:
        op = InstType::IALLOC8;
        break;
    default:
        break; // unreachable
    }
    auto inst = Inst::create(op, Type::L, ConstBits::get(bytes), nullptr);
    // alloca inst should be inserted at the beginning of the block
    _function->start->insts.push_back(inst);
    inst->to->id = _function->temp_counter++;
    return inst->to;
}

/**
 * @brief Create a compare equal instruction,
 * format: %to =w ceqw <lhs>, <rhs>
 */
ValuePtr IRBuilder::create_ceqw(ValuePtr lhs, ValuePtr rhs) {
    if (auto const_val = _folder.fold_eq(lhs, rhs); const_val) {
        return const_val;
    }
    auto inst = Inst::create(InstType::ICEQW, Type::W, lhs, rhs);
    return insert_inst(inst);
}

/**
 * @brief Create a compare not equal instruction,
 * format: %to =w cnew <lhs>, <rhs>
 */
ValuePtr IRBuilder::create_cnew(ValuePtr lhs, ValuePtr rhs) {
    if (auto const_val = _folder.fold_ne(lhs, rhs); const_val) {
        return const_val;
    }
    auto inst = Inst::create(InstType::ICNEW, Type::W, lhs, rhs);
    return insert_inst(inst);
}

/**
 * @brief Create a compare signed less or equal instruction,
 * format: %to =w cslew <lhs>, <rhs>
 */
ValuePtr IRBuilder::create_cslew(ValuePtr lhs, ValuePtr rhs) {
    if (auto const_val = _folder.fold_le(lhs, rhs); const_val) {
        return const_val;
    }
    auto inst = Inst::create(InstType::ICSLEW, Type::W, lhs, rhs);
    return insert_inst(inst);
}

/**
 * @brief Create a compare signed less than instruction,
 * format: %to =w csltw <lhs>, <rhs>
 */
ValuePtr IRBuilder::create_csltw(ValuePtr lhs, ValuePtr rhs) {
    if (auto const_val = _folder.fold_lt(lhs, rhs); const_val) {
        return const_val;
    }
    auto inst = Inst::create(InstType::ICSLTW, Type::W, lhs, rhs);
    return insert_inst(inst);
}

/**
 * @brief Create a compare signed greater or equal instruction,
 * format: %to =w csgew <lhs>, <rhs>
 */
ValuePtr IRBuilder::create_csgew(ValuePtr lhs, ValuePtr rhs) {
    if (auto const_val = _folder.fold_ge(lhs, rhs); const_val) {
        return const_val;
    }
    auto inst = Inst::create(InstType::ICSGEW, Type::W, lhs, rhs);
    return insert_inst(inst);
}

/**
 * @brief Create a compare signed greater than instruction,
 * format: %to =w csgtw <lhs>, <rhs>
 */
ValuePtr IRBuilder::create_csgtw(ValuePtr lhs, ValuePtr rhs) {
    if (auto const_val = _folder.fold_gt(lhs, rhs); const_val) {
        return const_val;
    }
    auto inst = Inst::create(InstType::ICSGTW, Type::W, lhs, rhs);
    return insert_inst(inst);
}

/**
 * @brief Create a compare float equal instruction,
 * format: %to =w ceqs <lhs>, <rhs>
 */
ValuePtr IRBuilder::create_ceqs(ValuePtr lhs, ValuePtr rhs) {
    if (auto const_val = _folder.fold_eq(lhs, rhs); const_val) {
        return const_val;
    }
    auto inst = Inst::create(InstType::ICEQS, Type::W, lhs, rhs);
    return insert_inst(inst);
}

/**
 * @brief Create a compare float not equal instruction,
 * format: %to =w cnes <lhs>, <rhs>
 */
ValuePtr IRBuilder::create_cnes(ValuePtr lhs, ValuePtr rhs) {
    if (auto const_val = _folder.fold_ne(lhs, rhs); const_val) {
        return const_val;
    }
    auto inst = Inst::create(InstType::ICNES, Type::W, lhs, rhs);
    return insert_inst(inst);
}

/**
 * @brief Create a compare float less or equal instruction,
 * format: %to =w cles <lhs>, <rhs>
 */
ValuePtr IRBuilder::create_cles(ValuePtr lhs, ValuePtr rhs) {
    if (auto const_val = _folder.fold_le(lhs, rhs); const_val) {
        return const_val;
    }
    auto inst = Inst::create(InstType::ICLES, Type::W, lhs, rhs);
    return insert_inst(inst);
}

/**
 * @brief Create a compare float less than instruction,
 * format: %to =w clts <lhs>, <rhs>
 */
ValuePtr IRBuilder::create_clts(ValuePtr lhs, ValuePtr rhs) {
    if (auto const_val = _folder.fold_lt(lhs, rhs); const_val) {
        return const_val;
    }
    auto inst = Inst::create(InstType::ICLTS, Type::W, lhs, rhs);
    return insert_inst(inst);
}

/**
 * @brief Create a compare float greater or equal instruction,
 * format: %to =w cges <lhs>, <rhs>
 */
ValuePtr IRBuilder::create_cges(ValuePtr lhs, ValuePtr rhs) {
    if (auto const_val = _folder.fold_ge(lhs, rhs); const_val) {
        return const_val;
    }
    auto inst = Inst::create(InstType::ICGES, Type::W, lhs, rhs);
    return insert_inst(inst);
}

/**
 * @brief Create a compare float greater than instruction,
 * format: %to =w cgts <lhs>, <rhs>
 */
ValuePtr IRBuilder::create_cgts(ValuePtr lhs, ValuePtr rhs) {
    if (auto const_val = _folder.fold_gt(lhs, rhs); const_val) {
        return const_val;
    }
    auto inst = Inst::create(InstType::ICGTS, Type::W, lhs, rhs);
    return insert_inst(inst);
}

/**
 * @brief Create a sign extension instruction,
 * format: %to =w extsw <value>
 */
ValuePtr IRBuilder::create_extsw(ValuePtr value) {
    if (auto const_val = _folder.fold_extsw(value); const_val) {
        return const_val;
    }
    auto inst = Inst::create(InstType::IEXTSW, Type::L, value, nullptr);
    return insert_inst(inst);
}

/**
 * @brief Create a float to signed integer instruction,
 * format: %to =w stosi <value>
 */
ValuePtr IRBuilder::create_stosi(ValuePtr value) {
    if (auto const_val = _folder.fold_stosi(value); const_val) {
        return const_val;
    }
    auto inst = Inst::create(InstType::ISTOSI, Type::W, value, nullptr);
    return insert_inst(inst);
}

/**
 * @brief Create a signed integer to float instruction,
 * format: %to =s stof <value>
 */
ValuePtr IRBuilder::create_swtof(ValuePtr value) {
    if (auto const_val = _folder.fold_swtof(value); const_val) {
        return const_val;
    }
    auto inst = Inst::create(InstType::ISWTOF, Type::S, value, nullptr);
    return insert_inst(inst);
}

/**
 * @brief Create a call instruction,
 * format: %to =<ty> call <name>(<args>)
 */
ValuePtr IRBuilder::create_call(Type ty, ValuePtr func,
                                std::vector<ValuePtr> args) {
    for (auto &arg : args) {
        insert_inst(Inst::create(InstType::IARG, Type::X, arg, nullptr));
    }
    auto inst = Inst::create(InstType::ICALL, ty, func, nullptr);
    return insert_inst(inst);
}

/**
 * @brief Create a return instruction,
 * format: ret <value>
 */
std::shared_ptr<Block> IRBuilder::create_ret(ValuePtr value) {
    auto insert_point = get_insert_point();
    if (insert_point->jump.type != Jump::NONE) {
        // dead code, just skip this instruction
        return insert_point;
    }
    insert_point->jump.type = Jump::RET;
    insert_point->jump.arg = value;
    return insert_point;
}

/**
 * @brief Create a jump instruction,
 * format: jmp <target>
 */
std::shared_ptr<Block> IRBuilder::create_jmp(std::shared_ptr<Block> target) {
    auto insert_point = get_insert_point();
    if (insert_point->jump.type != Jump::NONE) {
        // dead code, just skip this instruction
        return insert_point;
    }
    insert_point->jump.type = Jump::JMP;
    insert_point->jump.blk[0] = target;
    return insert_point;
}

/**
 * @brief Create a jump if zero instruction,
 * format: jz <cond> <if_true> <if_false>
 */
std::shared_ptr<Block> IRBuilder::create_jnz(ValuePtr cond,
                                             std::shared_ptr<Block> if_true,
                                             std::shared_ptr<Block> if_false) {
    auto insert_point = get_insert_point();
    if (insert_point->jump.type != Jump::NONE) {
        // dead code, just skip this instruction
        return insert_point;
    }
    insert_point->jump.type = Jump::JNZ;
    insert_point->jump.arg = cond;
    insert_point->jump.blk[0] = if_true;
    insert_point->jump.blk[1] = if_false;
    return insert_point;
}

} // namespace ir