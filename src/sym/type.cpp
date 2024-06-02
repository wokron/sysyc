#include "sym/type.h"

namespace sym {

bool IndirectType::operator==(const Type &other) const {
    if (!other.is_array() && !other.is_pointer()) {
        return false;
    }

    const auto &other_ref = dynamic_cast<const IndirectType &>(other);
    return *get_base_type() == *(other_ref.get_base_type());
}

std::shared_ptr<Type> Int32Type::implicit_cast(const Type &other) const {
    if (other.is_int32()) {
        return Int32Type::get();
    } else if (other.is_float()) {
        return FloatType::get();
    } else {
        return std::shared_ptr<Type>();
    }
}

std::shared_ptr<Type> FloatType::implicit_cast(const Type &other) const {
    if (other.is_int32() || other.is_float()) {
        return FloatType::get();
    } else {
        return std::shared_ptr<Type>();
    }
}

} // namespace sym
