#include "midend/type.h"

Type::Type(TypeId tid) : tid(tid) {}

bool VoidType::operator==(const Type &other) const {
    return tid == other.tid; // Check if type tags are the same
}
bool VoidType::operator!=(const Type &other) const { return !(*this == other); }

std::ostream &operator<<(std::ostream &os, const VoidType &type) {
    os << "Void Type";
    return os;
}

bool Int32Type::operator==(const Type &other) const {
    return tid == other.tid; // Check if type tags are the same
}
bool Int32Type::operator!=(const Type &other) const {
    return !(*this == other);
}

std::ostream &operator<<(std::ostream &os, const Int32Type &type) {
    os << "Int32 Type";
    return os;
}

bool FloatType::operator==(const Type &other) const {
    return tid == other.tid; // Check if type tags are the same
}
bool FloatType::operator!=(const Type &other) const {
    return !(*this == other);
}

std::ostream &operator<<(std::ostream &os, const FloatType &type) {
    os << "Float Type";
    return os;
}

ArrayType::ArrayType(int size, std::shared_ptr<Type> ele_type)
    : Type(TypeId::array_type), size(size), ele_type(ele_type) {}

bool ArrayType::operator==(const ArrayType &other) const {
    if (this->size != other.size)
        return false;
    return *this->ele_type == *other.ele_type;
}

bool ArrayType::operator!=(const ArrayType &other) const {
    return !(*this == other);
}

bool ArrayType::operator==(const Type &other) const {
    if (other.isArray()) {
        const auto &other_array = dynamic_cast<const ArrayType &>(other);
        return *this == other_array;
    }
    return false;
}

bool ArrayType::operator!=(const Type &other) const {
    return !(*this == other);
}

std::ostream &operator<<(std::ostream &os, const ArrayType &type) {
    os << "ArrayType(size=" << type.size << ")";
    return os;
}

int ArrayType::get_array_size() const { return size; }

Type *ArrayType::get_element_type() const { return ele_type.get(); }

PointerType::PointerType(std::shared_ptr<Type> ptr_type)
    : Type(TypeId::pointer_type), ptr_type(ptr_type) {}

bool PointerType::operator==(const PointerType &other) const {
    return *ptr_type == *other.ptr_type;
}

bool PointerType::operator!=(const PointerType &other) const {
    return !(*this == other);
}

bool PointerType::operator==(const Type &other) const {
    if (other.isPointer()) {
        const auto &other_pointer = dynamic_cast<const PointerType &>(other);
        return *this == other_pointer;
    }
    return false;
}

bool PointerType::operator!=(const Type &other) const {
    return !(*this == other);
}

Type *PointerType::get_ptr_type() const { return ptr_type.get(); }

std::ostream &operator<<(std::ostream &os, const PointerType &type) {
    os << "PointerType(ptr_type=";
    // os << *(type.get_ptr_type()); // Assuming Type has an overloaded <<
    // operator
    os << ")";
    return os;
}