#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <vector>

class Type {
  public:
    enum TypeId {
        VOID,
        INT32,
        FLOAT,
        POINTER,
        ARRAY,
    };

    Type(TypeId tid) : _tid(tid) {}

    bool is_void() const { return this->_tid == VOID; }

    bool is_int32() const { return this->_tid == INT32; }

    bool is_float() const { return this->_tid == FLOAT; }

    bool is_pointer() const { return this->_tid == POINTER; }

    bool is_array() const { return this->_tid == ARRAY; }

    virtual int get_size() const = 0;

    virtual bool operator==(const Type &other) const = 0;

    virtual std::string tostring() const = 0;

    bool operator!=(const Type &other) const { return !(*this == other); }

    TypeId get_type_id() { return this->_tid; }

  private:
    TypeId _tid = VOID;
};

class VoidType : public Type {
  public:
    VoidType() : Type(TypeId::VOID) {}

    int get_size() const override { return 1; }

    bool operator==(const Type &other) const override {
        return other.is_void();
    }

    std::string tostring() const override { return "void"; }
};

class Int32Type : public Type {
  public:
    Int32Type() : Type(TypeId::INT32) {}

    int get_size() const override { return 4; }

    bool operator==(const Type &other) const override {
        return other.is_int32();
    }

    std::string tostring() const override { return "int"; }
};

class FloatType : public Type {
  public:
    FloatType() : Type(TypeId::FLOAT) {}

    int get_size() const override { return 4; }

    bool operator==(const Type &other) const override {
        return other.is_float();
    }

    std::string tostring() const override { return "float"; }
};

class ReferenceType : public Type {
  public:
    ReferenceType(TypeId tid) : Type(tid) {}

    virtual std::shared_ptr<Type> get_ref_type() const = 0;

    bool operator==(const Type &other) const override;
};

class PointerType : public ReferenceType {
  public:
    PointerType(std::shared_ptr<Type> ref_type)
        : ReferenceType(TypeId::POINTER), _ref_type(ref_type) {}

    int get_size() const override { return 8; }

    std::shared_ptr<Type> get_ref_type() const override { return _ref_type; }

    std::string tostring() const override {
        return "*" + _ref_type->tostring();
    }

  private:
    std::shared_ptr<Type> _ref_type;
};

class ArrayType : public ReferenceType {
  public:
    ArrayType(int size, std::shared_ptr<Type> elm_type)
        : ReferenceType(TypeId::ARRAY), _size(size), _elm_type(elm_type) {}

    int get_size() const override { return _size * _elm_type->get_size(); }

    std::shared_ptr<Type> get_ref_type() const override { return _elm_type; }

    std::string tostring() const override {
        return "[" + std::to_string(_size) + "]" + _elm_type->tostring();
    }

  private:
    int _size = 0;

    std::shared_ptr<Type> _elm_type;
};

class TypeBuilder {
  public:
    TypeBuilder(std::shared_ptr<Type> base_type) : _type(base_type) {}

    TypeBuilder &in_array(int size) {
        _type = std::make_shared<ArrayType>(size, _type);
        return *this;
    }

    TypeBuilder &in_ptr() {
        _type = std::make_shared<PointerType>(_type);
        return *this;
    }

    std::shared_ptr<Type> get_type() const { return _type; }

  private:
    std::shared_ptr<Type> _type;
};
