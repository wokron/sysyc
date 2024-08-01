#include "ir/folder.h"
#include "ir/ir.h"

namespace ir {

ValuePtr Folder::fold_add(ValuePtr lhs, ValuePtr rhs) {
    auto lhs_const = _convert_to_const_bits(lhs);
    auto rhs_const = _convert_to_const_bits(rhs);
    if (lhs_const && rhs_const) {
        if (lhs_const->get_type() == Type::W) {
            return ConstBits::get(std::get<int>(lhs_const->value) +
                                  std::get<int>(rhs_const->value));
        } else if (lhs_const->get_type() == Type::S) {
            return ConstBits::get(std::get<float>(lhs_const->value) +
                                  std::get<float>(rhs_const->value));
        } else {
            return nullptr; // cannot fold
        }
    }

    if (lhs_const) {
        if (lhs_const->get_type() == Type::W &&
            std::get<int>(lhs_const->value) == 0) {
            return rhs;
        }
        if (lhs_const->get_type() == Type::S &&
            std::get<float>(lhs_const->value) == 0.0) {
            return rhs;
        }
    }

    if (rhs_const) {
        if (rhs_const->get_type() == Type::W &&
            std::get<int>(rhs_const->value) == 0) {
            return lhs;
        }
        if (rhs_const->get_type() == Type::S &&
            std::get<float>(rhs_const->value) == 0.0) {
            return lhs;
        }
    }

    return nullptr;
}

ValuePtr Folder::fold_sub(ValuePtr lhs, ValuePtr rhs) {
    if (lhs == rhs) {
        if (lhs->get_type() == Type::W) {
            return ConstBits::get(0);
        } else if (lhs->get_type() == Type::S) {
            return ConstBits::get(0.0f);
        }
    }

    auto lhs_const = _convert_to_const_bits(lhs);
    auto rhs_const = _convert_to_const_bits(rhs);
    if (lhs_const && rhs_const) {
        if (lhs_const->get_type() == Type::W) {
            return ConstBits::get(std::get<int>(lhs_const->value) -
                                  std::get<int>(rhs_const->value));
        } else if (lhs_const->get_type() == Type::S) {
            return ConstBits::get(std::get<float>(lhs_const->value) -
                                  std::get<float>(rhs_const->value));
        } else {
            return nullptr; // cannot fold
        }
    }

    if (lhs_const) {
        if (lhs_const->get_type() == Type::W &&
            std::get<int>(lhs_const->value) == 0) {
            return fold_neg(rhs);
        }
        if (lhs_const->get_type() == Type::S &&
            std::get<float>(lhs_const->value) == 0.0) {
            return fold_neg(rhs);
        }
    }

    if (rhs_const) {
        if (rhs_const->get_type() == Type::W &&
            std::get<int>(rhs_const->value) == 0) {
            return lhs;
        }
        if (rhs_const->get_type() == Type::S &&
            std::get<float>(rhs_const->value) == 0.0) {
            return lhs;
        }
    }

    return nullptr;
}

ValuePtr Folder::fold_neg(ValuePtr operand) {
    auto operand_const = _convert_to_const_bits(operand);
    if (operand_const) {
        if (operand_const->get_type() == Type::W) {
            return ConstBits::get(-std::get<int>(operand_const->value));
        } else if (operand_const->get_type() == Type::S) {
            return ConstBits::get(-std::get<float>(operand_const->value));
        } else {
            return nullptr; // cannot fold
        }
    }

    return nullptr;
}

ValuePtr Folder::fold_div(ValuePtr lhs, ValuePtr rhs) {
    if (lhs == rhs) {
        if (lhs->get_type() == Type::W) {
            return ConstBits::get(1);
        } else if (lhs->get_type() == Type::S) {
            return ConstBits::get(1.0f);
        }
    }

    auto lhs_const = _convert_to_const_bits(lhs);
    auto rhs_const = _convert_to_const_bits(rhs);
    if (lhs_const && rhs_const) {
        if (lhs_const->get_type() == Type::W) {
            return ConstBits::get(std::get<int>(lhs_const->value) /
                                  std::get<int>(rhs_const->value));
        } else if (lhs_const->get_type() == Type::S) {
            return ConstBits::get(std::get<float>(lhs_const->value) /
                                  std::get<float>(rhs_const->value));
        } else {
            return nullptr; // cannot fold
        }
    }

    if (lhs_const) {
        if (lhs_const->get_type() == Type::W &&
            std::get<int>(lhs_const->value) == 0) {
            return ConstBits::get(0);
        }
        if (lhs_const->get_type() == Type::S &&
            std::get<float>(lhs_const->value) == 0.0) {
            return ConstBits::get(0.0f);
        }
    }

    if (rhs_const) {
        if (rhs_const->get_type() == Type::W &&
            std::get<int>(rhs_const->value) == 1) {
            return lhs;
        }
        if (rhs_const->get_type() == Type::S &&
            std::get<float>(rhs_const->value) == 1.0) {
            return lhs;
        }
    }

    return nullptr;
}

ValuePtr Folder::fold_mul(ValuePtr lhs, ValuePtr rhs) {
    auto lhs_const = _convert_to_const_bits(lhs);
    auto rhs_const = _convert_to_const_bits(rhs);
    if (lhs_const && rhs_const) {
        if (lhs_const->get_type() == Type::W) {
            return ConstBits::get(std::get<int>(lhs_const->value) *
                                  std::get<int>(rhs_const->value));
        } else if (lhs_const->get_type() == Type::S) {
            return ConstBits::get(std::get<float>(lhs_const->value) *
                                  std::get<float>(rhs_const->value));
        } else {
            return nullptr; // cannot fold
        }
    }

    if (lhs_const) {
        if (lhs_const->get_type() == Type::W) {
            if (std::get<int>(lhs_const->value) == 0) {
                return ConstBits::get(0);
            }
            if (std::get<int>(lhs_const->value) == 1) {
                return rhs;
            }
        }
        if (lhs_const->get_type() == Type::S) {
            if (std::get<float>(lhs_const->value) == 0.0) {
                return ConstBits::get(0.0f);
            }
            if (std::get<float>(lhs_const->value) == 1.0) {
                return rhs;
            }
        }
    }

    if (rhs_const) {
        if (rhs_const->get_type() == Type::W) {
            if (std::get<int>(rhs_const->value) == 0) {
                return ConstBits::get(0);
            }
            if (std::get<int>(rhs_const->value) == 1) {
                return lhs;
            }
        }
        if (rhs_const->get_type() == Type::S) {
            if (std::get<float>(rhs_const->value) == 0.0) {
                return ConstBits::get(0.0f);
            }
            if (std::get<float>(rhs_const->value) == 1.0) {
                return lhs;
            }
        }
    }

    return nullptr;
}

ValuePtr Folder::fold_rem(ValuePtr lhs, ValuePtr rhs) {
    auto lhs_const = _convert_to_const_bits(lhs);
    auto rhs_const = _convert_to_const_bits(rhs);
    if (lhs_const && rhs_const) {
        if (lhs_const->get_type() == Type::W) {
            return ConstBits::get(std::get<int>(lhs_const->value) %
                                  std::get<int>(rhs_const->value));
        } else {
            return nullptr; // cannot fold
        }
    }

    if (lhs_const) {
        if (lhs_const->get_type() == Type::W &&
            std::get<int>(lhs_const->value) == 0) {
            return ConstBits::get(0);
        }
    }

    if (rhs_const) {
        if (rhs_const->get_type() == Type::W &&
            std::get<int>(rhs_const->value) == 1) {
            return lhs;
        }
    }

    return nullptr;
}

ValuePtr Folder::fold_eq(ValuePtr lhs, ValuePtr rhs) {
    auto lhs_const = _convert_to_const_bits(lhs);
    auto rhs_const = _convert_to_const_bits(rhs);
    if (lhs_const && rhs_const) {
        bool result;
        if (lhs_const->get_type() == Type::W) {
            result = std::get<int>(lhs_const->value) ==
                     std::get<int>(rhs_const->value);
        } else if (lhs_const->get_type() == Type::S) {
            result = std::get<float>(lhs_const->value) ==
                     std::get<float>(rhs_const->value);
        } else {
            return nullptr; // cannot fold
        }
        return ConstBits::get((int)(result));
    }

    return nullptr;
}

ValuePtr Folder::fold_ne(ValuePtr lhs, ValuePtr rhs) {
    auto lhs_const = _convert_to_const_bits(lhs);
    auto rhs_const = _convert_to_const_bits(rhs);
    if (lhs_const && rhs_const) {
        bool result;
        if (lhs_const->get_type() == Type::W) {
            result = std::get<int>(lhs_const->value) !=
                     std::get<int>(rhs_const->value);
        } else if (lhs_const->get_type() == Type::S) {
            result = std::get<float>(lhs_const->value) !=
                     std::get<float>(rhs_const->value);
        } else {
            return nullptr; // cannot fold
        }
        return ConstBits::get((int)(result));
    }

    return nullptr;
}

ValuePtr Folder::fold_lt(ValuePtr lhs, ValuePtr rhs) {
    auto lhs_const = _convert_to_const_bits(lhs);
    auto rhs_const = _convert_to_const_bits(rhs);
    if (lhs_const && rhs_const) {
        bool result;
        if (lhs_const->get_type() == Type::W) {
            result = std::get<int>(lhs_const->value) <
                     std::get<int>(rhs_const->value);
        } else if (lhs_const->get_type() == Type::S) {
            result = std::get<float>(lhs_const->value) <
                     std::get<float>(rhs_const->value);
        } else {
            return nullptr; // cannot fold
        }
        return ConstBits::get((int)(result));
    }

    return nullptr;
}

ValuePtr Folder::fold_le(ValuePtr lhs, ValuePtr rhs) {
    auto lhs_const = _convert_to_const_bits(lhs);
    auto rhs_const = _convert_to_const_bits(rhs);
    if (lhs_const && rhs_const) {
        bool result;
        if (lhs_const->get_type() == Type::W) {
            result = std::get<int>(lhs_const->value) <=
                     std::get<int>(rhs_const->value);
        } else if (lhs_const->get_type() == Type::S) {
            result = std::get<float>(lhs_const->value) <=
                     std::get<float>(rhs_const->value);
        } else {
            return nullptr; // cannot fold
        }
        return ConstBits::get((int)(result));
    }

    return nullptr;
}

ValuePtr Folder::fold_gt(ValuePtr lhs, ValuePtr rhs) {
    auto lhs_const = _convert_to_const_bits(lhs);
    auto rhs_const = _convert_to_const_bits(rhs);
    if (lhs_const && rhs_const) {
        bool result;
        if (lhs_const->get_type() == Type::W) {
            result = std::get<int>(lhs_const->value) >
                     std::get<int>(rhs_const->value);
        } else if (lhs_const->get_type() == Type::S) {
            result = std::get<float>(lhs_const->value) >
                     std::get<float>(rhs_const->value);
        } else {
            return nullptr; // cannot fold
        }
        return ConstBits::get((int)(result));
    }

    return nullptr;
}

ValuePtr Folder::fold_ge(ValuePtr lhs, ValuePtr rhs) {
    auto lhs_const = _convert_to_const_bits(lhs);
    auto rhs_const = _convert_to_const_bits(rhs);
    if (lhs_const && rhs_const) {
        bool result;
        if (lhs_const->get_type() == Type::W) {
            result = std::get<int>(lhs_const->value) >=
                     std::get<int>(rhs_const->value);
        } else if (lhs_const->get_type() == Type::S) {
            result = std::get<float>(lhs_const->value) >=
                     std::get<float>(rhs_const->value);
        } else {
            return nullptr; // cannot fold
        }
        return ConstBits::get((int)(result));
    }

    return nullptr;
}

ValuePtr Folder::fold_stosi(ValuePtr value) {
    if (auto const_val = std::dynamic_pointer_cast<ConstBits>(value);
        const_val) {
        return const_val->to_int();
    }
    return nullptr;
}

ValuePtr Folder::fold_swtof(ValuePtr value) {
    if (auto const_val = std::dynamic_pointer_cast<ConstBits>(value);
        const_val) {
        return const_val->to_float();
    }
    return nullptr;
}

ValuePtr Folder::fold_extsw(ValuePtr value) {
    if (auto const_val = std::dynamic_pointer_cast<ConstBits>(value);
        const_val) {
        return const_val;
    }
    return nullptr;
}

ConstBitsPtr Folder::_convert_to_const_bits(ValuePtr value) {
    if (auto constant = std::dynamic_pointer_cast<ConstBits>(value)) {
        return constant;
    }
    return nullptr;
}

} // namespace ir