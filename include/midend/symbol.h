#pragma once

#include "type.h"
#include <memory>
#include <unordered_map>
#include <vector>

class Symbol {
  public:
    std::string name;
    std::shared_ptr<Type> type;
    bool constant;

    Symbol(std::string name, std::shared_ptr<Type> type, bool constant)
        : name(std::move(name)), type(type), constant(constant){};
};

class SymbolTable {
  public:
    std::vector<std::unordered_map<std::string, std::shared_ptr<Symbol>>>
        symbol_table;
    static SymbolTable table;
    bool wait = false;

  public:
    SymbolTable() {
        symbol_table.emplace_back(); // 在vector中加入一个新的unordered_map
    }

    static SymbolTable &getInstance() { return table; }

    void addBlockLayer() {
        if (wait) {
            wait = false;
        } else {
            symbol_table.emplace_back();
        }
    }

    void removeBlockLayer() {
        if (symbol_table.size() > 2) {
            symbol_table.pop_back();
        }
    }

    void addFuncLayer() {
        symbol_table.emplace_back();
        wait = true;
    }

    void removeFuncLayer() { symbol_table.pop_back(); }

    void addSymbol(const std::string &name, std::shared_ptr<Symbol> symbol) {
        symbol_table.back()[name] = symbol;
    }

    std::shared_ptr<Symbol> getSymbol(const std::string &name) {
        for (int i = symbol_table.size() - 1; i >= 0; --i) {
            auto it = symbol_table[i].find(name);
            if (it != symbol_table[i].end()) {
                return it->second;
            }
        }
        return nullptr;
    }
};
