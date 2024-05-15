#include "midend/type.h"

bool ReferenceType::operator==(const Type &other) const {
    if (!other.is_array() || !other.is_pointer()) {
        return false;
    }

    const auto &other_ref = dynamic_cast<const ReferenceType &>(other);
    return *get_ref_type() == *(other_ref.get_ref_type());
}
