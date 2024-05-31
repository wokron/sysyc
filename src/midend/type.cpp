#include "midend/type.h"

bool IndirectType::operator==(const Type &other) const {
    if (!other.is_array() || !other.is_pointer()) {
        return false;
    }

    const auto &other_ref = dynamic_cast<const IndirectType &>(other);
    return *get_base_type() == *(other_ref.get_base_type());
}

std::shared_ptr<Type> Int32Type::implicit_cast(const Type &other) const {
    if (other.is_int32()) {
        return std::make_shared<Int32Type>();
    } else if (other.is_float()) {
        return std::make_shared<FloatType>();
    } else {
        return std::shared_ptr<Type>();
    }
}

std::shared_ptr<Type> FloatType::implicit_cast(const Type &other) const {
    if (other.is_int32() || other.is_float()) {
        return std::make_shared<FloatType>();
    } else {
        return std::shared_ptr<Type>();
    }
}