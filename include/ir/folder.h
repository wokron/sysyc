#pragma once

#include "ir.h"

namespace ir {

class Folder {
  public:
    ValuePtr fold_add(ValuePtr lhs, ValuePtr rhs);
    ValuePtr fold_sub(ValuePtr lhs, ValuePtr rhs);
    ValuePtr fold_neg(ValuePtr operand);
    ValuePtr fold_div(ValuePtr lhs, ValuePtr rhs);
    ValuePtr fold_mul(ValuePtr lhs, ValuePtr rhs);
    ValuePtr fold_rem(ValuePtr lhs, ValuePtr rhs);

    ValuePtr fold_eq(ValuePtr lhs, ValuePtr rhs);
    ValuePtr fold_ne(ValuePtr lhs, ValuePtr rhs);
    ValuePtr fold_lt(ValuePtr lhs, ValuePtr rhs);
    ValuePtr fold_le(ValuePtr lhs, ValuePtr rhs);
    ValuePtr fold_gt(ValuePtr lhs, ValuePtr rhs);
    ValuePtr fold_ge(ValuePtr lhs, ValuePtr rhs);

    ValuePtr fold_stosi(ValuePtr value);
    ValuePtr fold_swtof(ValuePtr value);
    ValuePtr fold_extsw(ValuePtr value);

  private:
    /**
     * @brief Convert a value to a constant bits pointer.
     * @return A constant bits pointer, or nullptr if the value is not a
     * constant.
     */
    static ConstBitsPtr _convert_to_const_bits(ValuePtr value);
};

} // namespace ir