#pragma once

#include "type.h"
#include <memory>
#include <unordered_map>

struct Symbol {
    std::string name;
    std::shared_ptr<Type> type;

    Symbol(std::string name, std::shared_ptr<Type> type)
        : name(name), type(type){};

    virtual std::string tostring() const = 0;
};

struct VariableSymbol : public Symbol {
    bool is_constant;

    VariableSymbol(std::string name, std::shared_ptr<Type> type,
                   bool is_constant)
        : Symbol(name, type), is_constant(is_constant){};

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
  public:
    std::unordered_map<std::string, std::shared_ptr<Symbol>> symbols;
    std::shared_ptr<SymbolTable> parent;

  public:
    SymbolTable(std::shared_ptr<SymbolTable> parent = nullptr)
        : parent(parent) {}

    bool exist_in_scope(const std::string name) {
        return symbols.find(name) != symbols.end();
    }

    void add_symbol(std::shared_ptr<Symbol> symbol) {
        if (!exist_in_scope(symbol->name)) {
            symbols[symbol->name] = symbol;
        }
        // if the name already exists, we will not override it
    }

    std::shared_ptr<Symbol> get_symbol(const std::string name) {
        auto it = symbols.find(name);
        if (it != symbols.end()) {
            return it->second;
        } else if (parent != nullptr) {
            return parent->get_symbol(name);
        } else {
            return nullptr;
        }
    }

    // create a new scope
    std::shared_ptr<SymbolTable> push_scope() {
        return std::make_shared<SymbolTable>(shared_from_this());
    }

    // return parent scope
    std::shared_ptr<SymbolTable> pop_scope() { return parent; }

    bool has_parent() const { return parent != nullptr; }
};
