#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace sym {

class Type {
  public:
    enum TypeId {
        ERROR,
        VOID,
        INT32,
        FLOAT,
        POINTER,
        ARRAY,
    };

    Type(TypeId tid) : _tid(tid) {}

    bool is_void() const { return this->_tid == VOID; }

    bool is_error() const { return this->_tid == ERROR; }

    bool is_int32() const { return this->_tid == INT32; }

    bool is_float() const { return this->_tid == FLOAT; }

    bool is_pointer() const { return this->_tid == POINTER; }

    bool is_array() const { return this->_tid == ARRAY; }

    /**
     * @brief Get the size of the type in bytes
     */
    virtual int get_size() const = 0;

    virtual bool operator==(const Type &other) const = 0;

    virtual std::string tostring() const = 0;

    bool operator!=(const Type &other) const { return !(*this == other); }

    TypeId get_type_id() { return this->_tid; }

    /**
     * @brief Check if the type can be implicitly casted to another type
     * @param other The target type
     * @return The type after implicit cast, or nullptr if the cast is invalid
     */
    virtual std::shared_ptr<Type> implicit_cast(const Type &other) const = 0;

  private:
    TypeId _tid = VOID;
};

class ErrorType : public Type {
  public:
    static std::shared_ptr<ErrorType> get() {
        static std::shared_ptr<ErrorType> instance(new ErrorType());
        return instance;
    }

    int get_size() const override { return -1; }

    bool operator==(const Type &other) const override { return false; }

    std::string tostring() const override { return "error-type"; }

    std::shared_ptr<Type> implicit_cast(const Type &other) const override {
        return ErrorType::get();
    }

  private:
    ErrorType() : Type(TypeId::ERROR) {}
};

class VoidType : public Type {
  public:
    static std::shared_ptr<VoidType> get() {
        static std::shared_ptr<VoidType> instance(new VoidType());
        return instance;
    }

    int get_size() const override { return 1; }

    bool operator==(const Type &other) const override {
        return other.is_void();
    }

    std::string tostring() const override { return "void"; }

    std::shared_ptr<Type> implicit_cast(const Type &other) const override {
        return ErrorType::get();
    }

  private:
    VoidType() : Type(TypeId::VOID) {}
};

class Int32Type : public Type {
  public:
    static std::shared_ptr<Int32Type> get() {
        static std::shared_ptr<Int32Type> instance(new Int32Type());
        return instance;
    }

    int get_size() const override { return 4; }

    bool operator==(const Type &other) const override {
        return other.is_int32();
    }

    std::string tostring() const override { return "int"; }

    std::shared_ptr<Type> implicit_cast(const Type &other) const override;

  private:
    Int32Type() : Type(TypeId::INT32) {}
};

class FloatType : public Type {
  public:
    static std::shared_ptr<FloatType> get() {
        static std::shared_ptr<FloatType> instance(new FloatType());
        return instance;
    }

    int get_size() const override { return 4; }

    bool operator==(const Type &other) const override {
        return other.is_float();
    }

    std::string tostring() const override { return "float"; }

    std::shared_ptr<Type> implicit_cast(const Type &other) const override;

  private:
    FloatType() : Type(TypeId::FLOAT) {}
};

class IndirectType : public Type {
  public:
    IndirectType(TypeId tid) : Type(tid) {}

    virtual std::shared_ptr<Type> get_base_type() const = 0;

    bool operator==(const Type &other) const override;

    std::shared_ptr<Type> implicit_cast(const Type &other) const override {
        return ErrorType::get();
    }
};

class PointerType : public IndirectType {
  public:
    PointerType(std::shared_ptr<Type> ref_type)
        : IndirectType(TypeId::POINTER), _ref_type(ref_type) {}

    int get_size() const override { return 8; }

    std::shared_ptr<Type> get_base_type() const override { return _ref_type; }

    std::string tostring() const override {
        return "*" + _ref_type->tostring();
    }

  private:
    std::shared_ptr<Type> _ref_type;
};

class ArrayType : public IndirectType {
  public:
    ArrayType(int size, std::shared_ptr<Type> elm_type)
        : IndirectType(TypeId::ARRAY), _size(size), _elm_type(elm_type) {}

    int get_size() const override { return _size * _elm_type->get_size(); }

    std::shared_ptr<Type> get_base_type() const override { return _elm_type; }

    int get_total_elm_count() const {
        if (_elm_type->is_array()) {
            return _size * std::dynamic_pointer_cast<ArrayType>(_elm_type)
                               ->get_total_elm_count();
        } else {
            return _size;
        }
    }

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

// some alias for types
using TypePtr = std::shared_ptr<Type>;
using ErrorTypePtr = std::shared_ptr<ErrorType>;
using VoidTypePtr = std::shared_ptr<VoidType>;
using Int32TypePtr = std::shared_ptr<Int32Type>;
using FloatTypePtr = std::shared_ptr<FloatType>;
using PointerTypePtr = std::shared_ptr<PointerType>;
using ArrayTypePtr = std::shared_ptr<ArrayType>;

} // namespace sym
