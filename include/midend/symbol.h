#pragma once

#include "type.h"
#include <memory>
#include <unordered_map>

class Symbol {
  public:
    std::string name;
    std::shared_ptr<Type> type;
    bool constant;

    Symbol(std::string name, std::shared_ptr<Type> type, bool constant)
        : name(std::move(name)), type(type), constant(constant){};
};

class SymbolTable : public std::enable_shared_from_this<SymbolTable> {
  public:
    std::unordered_map<std::string, std::shared_ptr<Symbol>> symbols;
    std::shared_ptr<SymbolTable> parent;

  public:
    SymbolTable(std::shared_ptr<SymbolTable> parent = nullptr)
        : parent(parent) {}

    void addSymbol(const std::string &name, std::shared_ptr<Symbol> symbol) {
        symbols[name] = symbol;
    }

    std::shared_ptr<Symbol> getSymbol(const std::string &name) {
        auto it = symbols.find(name);
        if (it != symbols.end()) {
            return it->second;
        } else if (parent != nullptr) {
            return parent->getSymbol(name);
        }
        return nullptr;
    }

    // create a new scope
    std::shared_ptr<SymbolTable> pushScope() {
        return std::make_shared<SymbolTable>(shared_from_this());
    }

    // return parent scope
    std::shared_ptr<SymbolTable> popScope() { return parent; }

    bool hasParent() const { return parent != nullptr; }
};
