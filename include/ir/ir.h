#pragma once

#include "utils.h"
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace ir {

enum Type {
    X = 'x',
    W = 'w',
    L = 'l',
    S = 's',
};

inline std::string type_to_string(Type ty) {
    std::string str(1, (char)ty);
    return str;
}

struct Value {
    virtual void emit(std::ostream &out) const = 0;

    virtual Type get_type() const = 0;
};

struct Const : public Value {};

struct Address : public Const {
    std::string name;

    Address(std::string name) : name(name) {}

    static std::shared_ptr<Address> get(std::string name);

    void emit(std::ostream &out) const override { out << "$" << name; }

    Type get_type() const override { return Type::L; }

  private:
    static std::unordered_map<std::string, std::shared_ptr<Address>>
        _addrcon_cache;
};

struct ConstBits : public Const {
    std::variant<int, float> value;

    template <typename T> ConstBits(T value) : value(value) {}

    template <typename T> static std::shared_ptr<ConstBits> get(T value);

    void emit(std::ostream &out) const override;

    Type get_type() const override;

    std::shared_ptr<ConstBits> to_int() const {
        if (auto int_val = std::get_if<int>(&value); int_val) {
            return ConstBits::get(*int_val);
        } else if (auto float_val = std::get_if<float>(&value); float_val) {
            return ConstBits::get((int)*float_val);
        } else {
            throw std::logic_error("variant empty");
        }
    }

    std::shared_ptr<ConstBits> to_float() const {
        if (auto int_val = std::get_if<int>(&value); int_val) {
            return ConstBits::get((float)*int_val);
        } else if (auto float_val = std::get_if<float>(&value); float_val) {
            return ConstBits::get(*float_val);
        } else {
            throw std::logic_error("variant empty");
        }
    }

  private:
    static std::unordered_map<float, std::shared_ptr<ConstBits>>
        _floatcon_cache;
    static std::unordered_map<int, std::shared_ptr<ConstBits>> _intcon_cache;
};

template <typename T>
inline std::shared_ptr<ConstBits> ConstBits::get(T value) {
    if constexpr (std::is_same_v<T, int>) {
        if (auto it = _intcon_cache.find(value); it != _intcon_cache.end()) {
            return it->second;
        } else {
            return _intcon_cache[value] = std::make_shared<ConstBits>(value);
        }
    } else if constexpr (std::is_same_v<T, float>) {
        if (auto it = _floatcon_cache.find(value);
            it != _floatcon_cache.end()) {
            return it->second;
        } else {
            return _floatcon_cache[value] = std::make_shared<ConstBits>(value);
        }
    }
}

enum InstType {

#define OP(op, name) op,
#include "ops.h"
#undef OP

};

struct Temp;

struct Inst {
    InstType insttype;
    std::shared_ptr<Temp> to;
    std::shared_ptr<Value> arg[2];

    static std::shared_ptr<Inst> create(InstType insttype, Type ty,
                                        std::shared_ptr<Value> arg0,
                                        std::shared_ptr<Value> arg1);

    void emit(std::ostream &out) const;
};

struct Block;

struct Phi {
    Type ty;
    std::shared_ptr<Temp> to;
    std::vector<std::pair<std::shared_ptr<Block>, std::shared_ptr<Value>>> args;

    Phi(Type ty, std::shared_ptr<Temp> to, decltype(args) args)
        : ty(ty), to(to), args(args) {}

    void emit(std::ostream &out) const;
};

struct Jump {
    enum {
        NONE, // none means fall through
        JMP,
        JNZ,
        RET,
    } type;

    std::shared_ptr<Value> arg;
    std::shared_ptr<Block> blk[2];
};

struct Function;

struct Block {
    uint id = 0;
    std::string name; // name without @

    std::vector<std::shared_ptr<Phi>> phis;
    std::vector<std::shared_ptr<Inst>> insts;
    Jump jump;

    std::shared_ptr<Block> next;

    static std::shared_ptr<Block> create(std::string name, Function &func);

    void emit(std::ostream &out) const;

    std::string get_name() const {
        return name + (id ? "." + std::to_string(id) : "");
    }
};

struct Module;

struct Function {
    bool is_export;
    std::string name; // name without $
    Type ty;
    std::shared_ptr<Block> start, end;

    // use a counter to generate unique temp name in function scope
    uint temp_counter = 1;
    uint *block_counter_ptr;

    using TempPtrList = std::vector<std::shared_ptr<Temp>>;
    using FunctionPtr = std::shared_ptr<Function>;

    static std::tuple<FunctionPtr, TempPtrList>
    create(bool is_export, std::string name, Type ty, std::vector<Type> params,
           Module &module);

    void add_block(std::shared_ptr<Block> blk) {
        end->next = blk;
        end = blk;
    }

    void emit(std::ostream &out) const;

    std::shared_ptr<Address> get_address() const { return Address::get(name); }
};

struct PhiUse {
    std::shared_ptr<Phi> phi;
    std::shared_ptr<Block> blk;
};

struct InstUse {
    std::shared_ptr<Inst> ins;
};

struct JmpUse {
    std::shared_ptr<Block> blk;
};

using Use = std::variant<PhiUse, InstUse, JmpUse>;

struct Temp : public Value {
    uint id = 0;
    std::string name;
    Type type;
    std::shared_ptr<Inst> def;
    std::vector<Use> uses;

    Temp(std::string name, Type type, std::shared_ptr<Inst> def)
        : name(name), type(type), def(def) {}

    void emit(std::ostream &out) const override {
        out << "%" << name;
        if (id) {
            out << "." << std::to_string(id);
        }
    }

    Type get_type() const override { return type; }
};

struct DataItem {
    virtual void emit(std::ostream &out) const = 0;
    virtual ~DataItem() = default;
};

struct ConstData : public DataItem {
    Type ty;
    std::vector<std::shared_ptr<Const>> values;

    ConstData(Type ty, std::vector<std::shared_ptr<Const>> values)
        : ty(ty), values(values) {}

    void emit(std::ostream &out) const override;
};

struct ZeroData : public DataItem {
    int bytes;

    ZeroData(int bytes) : bytes(bytes) {}

    void emit(std::ostream &out) const override { out << "z " << bytes; }
};

struct Data {
    bool is_export;
    std::string name; // name without $
    int align;
    std::vector<std::unique_ptr<DataItem>> items;

    static std::shared_ptr<Data> create(bool is_export, std::string name,
                                        int align, Module &module);

    Data &append_const(Type ty, std::vector<std::shared_ptr<Const>> values);

    Data &append_zero(int bytes);

    void emit(std::ostream &out) const;

    std::shared_ptr<Address> get_address() const { return Address::get(name); }
};

struct Module {
    std::vector<std::shared_ptr<Data>> datas;
    std::vector<std::shared_ptr<Function>> functions;
    uint block_counter = 1;

    void add_function(std::shared_ptr<Function> func) {
        functions.push_back(func);
    }

    void add_data(std::shared_ptr<Data> data) { datas.push_back(data); }

    void emit(std::ostream &out) const;
};

// some alias for easier use
using TempPtr = std::shared_ptr<Temp>;
using ValuePtr = std::shared_ptr<Value>;
using ConstPtr = std::shared_ptr<Const>;
using ConstBitsPtr = std::shared_ptr<ConstBits>;
using AddressPtr = std::shared_ptr<Address>;
using InstPtr = std::shared_ptr<Inst>;
using BlockPtr = std::shared_ptr<Block>;
using PhiPtr = std::shared_ptr<Phi>;
using FunctionPtr = std::shared_ptr<Function>;
using DataPtr = std::shared_ptr<Data>;

} // namespace ir
