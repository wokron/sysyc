#pragma once

#include "ir/ir.h"
#include "type.h"
#include <memory>
#include <unordered_map>

struct Symbol {
    std::string name;
    std::shared_ptr<Type> type;
    std::shared_ptr<ir::Value> value;

    Symbol(std::string name, std::shared_ptr<Type> type)
        : name(name), type(type){};

    virtual std::string tostring() const = 0;
};

class Initializer {
  public:
    Initializer(int space = 1) : _space(space) {}

    bool insert(std::shared_ptr<ir::Value> value) {
        if (_pos >= _space) {
            return false;
        }
        _init_values[_pos++] = value;
        return true;
    }

    bool insert(Initializer &init) {
        bool rt = true;
        for (auto &kv : init._init_values) {
            int pos = _pos + kv.first;
            if (pos >= _space) {
                rt = false;
                continue;
            }
            _init_values[pos] = kv.second;
        }
        _pos += init._space;

        return rt;
    }

    std::unordered_map<int, std::shared_ptr<ir::Value>> get_values() {
        return _init_values;
    }

  private:
    std::unordered_map<int, std::shared_ptr<ir::Value>> _init_values;
    int _space;
    int _pos = 0;
};

struct VariableSymbol : public Symbol {
    bool is_constant;
    std::shared_ptr<Initializer> initializer; // nullable

    VariableSymbol(std::string name, std::shared_ptr<Type> type,
                   bool is_constant, std::shared_ptr<Initializer> initializer)
        : Symbol(name, type), is_constant(is_constant),
          initializer(initializer){};

    std::string tostring() const override {
        return (is_constant ? "const " : "var ") + name + " " +
               type->tostring();
    }
};

struct FunctionSymbol : public Symbol {
    std::vector<std::shared_ptr<Type>> param_types;

    FunctionSymbol(std::string name,
                   std::vector<std::shared_ptr<Type>> param_types,
                   std::shared_ptr<Type> return_type)
        : Symbol(name, return_type), param_types(param_types){};

    std::string tostring() const override {
        std::string rt;
        rt.append("func ").append(name).append("(");
        for (auto &param_type : param_types) {
            if (&param_type != &param_types[0]) {
                rt.append(", ");
            }
            rt.append(param_type->tostring());
        }
        rt.append(") ").append(type->tostring());
        return rt;
    }
};

class SymbolTable : public std::enable_shared_from_this<SymbolTable> {
  private:
    std::unordered_map<std::string, std::shared_ptr<Symbol>> _symbols;
    std::shared_ptr<SymbolTable> _parent;

  public:
    SymbolTable() : _parent(nullptr) {}
    SymbolTable(std::shared_ptr<SymbolTable> parent) : _parent(parent) {}

    bool exist_in_scope(const std::string name) {
        return _symbols.find(name) != _symbols.end();
    }

    bool add_symbol(std::shared_ptr<Symbol> symbol) {
        if (exist_in_scope(symbol->name)) {
            return false;
        }
        _symbols[symbol->name] = symbol;
        return true;
    }

    std::shared_ptr<Symbol> get_symbol(const std::string name) {
        auto it = _symbols.find(name);
        if (it != _symbols.end()) {
            return it->second;
        } else if (_parent != nullptr) {
            return _parent->get_symbol(name);
        } else {
            return nullptr;
        }
    }

    // create a new scope
    std::shared_ptr<SymbolTable> push_scope() {
        return std::make_shared<SymbolTable>(shared_from_this());
    }

    // return parent scope
    std::shared_ptr<SymbolTable> pop_scope() { return _parent; }

    bool has_parent() const { return _parent != nullptr; }

    std::string tostring() const {
        std::string rt;
        if (_parent != nullptr) {
            rt.append(_parent->tostring());
        }

        rt.append("==========");
        for (auto &symbol : _symbols) {
            rt.append(symbol.second->tostring()).append("\n");
        }
        return rt;
    }
};
