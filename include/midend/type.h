#pragma once

#include <iostream>
#include <memory>
#include <string>

class Type {
  public:
    enum TypeId {
        void_type,
        int1_type,
        int32_type,
        float_type,
        pointer_type,
        array_type
    };

    virtual ~Type() = default;

    explicit Type(TypeId tid);

    bool isVoid() const { return this->tid == void_type; }

    bool isInt1() const { return this->tid == int1_type; }

    bool isInt32() const { return this->tid == int32_type; }

    bool isFloat() const { return this->tid == float_type; }

    bool isPointer() const { return this->tid == pointer_type; }

    bool isArray() const { return this->tid == array_type; }

    virtual bool operator==(const Type &other) const;

    virtual bool operator!=(const Type &other) const;

    TypeId getTypeID() { return this->tid; }

  public:
    TypeId tid = void_type;
};

class VoidType : public Type {
  public:
    VoidType() : Type(TypeId::void_type) {}

    ~VoidType() override = default;

    bool operator==(const VoidType &other) const;

    bool operator!=(const VoidType &other) const;

    bool operator==(const Type &other) const override;

    bool operator!=(const Type &other) const override;

    friend std::ostream &operator<<(std::ostream &os, const VoidType &type);
};

class Int1Type : public Type {
  public:
    Int1Type() : Type(TypeId::int1_type) {}

    ~Int1Type() override = default;

    bool operator==(const Type &other) const override;

    bool operator!=(const Type &other) const override;

    friend std::ostream &operator<<(std::ostream &os, const Int1Type &type);
};

class Int32Type : public Type {
  public:
    Int32Type() : Type(TypeId::int32_type) {}

    ~Int32Type() override = default;

    bool operator==(const Type &other) const override;

    bool operator!=(const Type &other) const override;

    friend std::ostream &operator<<(std::ostream &os, const Int32Type &type);
};

class FloatType : public Type {
  public:
    FloatType() : Type(TypeId::float_type) {}

    ~FloatType() override = default;

    bool operator==(const Type &other) const override;

    bool operator!=(const Type &other) const override;

    friend std::ostream &operator<<(std::ostream &os, const FloatType &type);
};

class PointerType : public Type {
  public:
    std::shared_ptr<Type> ptr_type;

    PointerType(std::shared_ptr<Type> ptr_type);

    ~PointerType() override = default;

    bool operator==(const PointerType &other) const;

    bool operator!=(const PointerType &other) const;

    bool operator==(const Type &other) const override;

    bool operator!=(const Type &other) const override;

    friend std::ostream &operator<<(std::ostream &os, const PointerType &type);

    Type *get_ptr_type() const;
};

class ArrayType : public Type {

  public:
    int size = 0;

    std::shared_ptr<Type> ele_type;

  public:
    ArrayType(int size, std::shared_ptr<Type> ele_type);

    ~ArrayType() override = default;

    bool operator==(const ArrayType &other) const;

    bool operator!=(const ArrayType &other) const;

    bool operator==(const Type &other) const;

    bool operator!=(const Type &other) const;

    friend std::ostream &operator<<(std::ostream &os, const ArrayType &type);

    int get_array_size() const;

    Type *get_element_type() const;
};
