#include "midend/type.h"

bool IndirectType::operator==(const Type &other) const {
    if (!other.is_array() || !other.is_pointer()) {
        return false;
    }

    const auto &other_ref = dynamic_cast<const IndirectType &>(other);
    return *get_base_type() == *(other_ref.get_base_type());
}
