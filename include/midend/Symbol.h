#ifndef SYSYC_SYMBOL_H
#define SYSYC_SYMBOL_H

#include "Type.h"
#include <memory>
#include <vector>
#include <unordered_map>

class Symbol
{
public:
    std::string name;
    std::shared_ptr<Type> type;
    bool constant;

    Symbol(std::string name, std::shared_ptr<Type> type, bool constant)
        : name(std::move(name)), type(type), constant(constant){};
};

class SymbolTable
{
public:
    std::vector<std::unordered_map<std::string, std::shared_ptr<Symbol>>> symbolTable;
    static SymbolTable table;
    bool wait = false;

public:
    SymbolTable()
    {
        symbolTable.emplace_back(); // 在vector中加入一个新的unordered_map
    }

    static SymbolTable &getInstance()
    {
        return table;
    }

    void addBlockLayer()
    {
        if (wait)
        {
            wait = false;
        }
        else
        {
            symbolTable.emplace_back();
        }
    }

    void removeBlockLayer()
    {
        if (symbolTable.size() > 2)
        {
            symbolTable.pop_back();
        }
    }

    void addFuncLayer()
    {
        symbolTable.emplace_back();
        wait = true;
    }

    void removeFuncLayer()
    {
        symbolTable.pop_back();
    }

    void addSymbol(const std::string &name, std::shared_ptr<Symbol> symbol)
    {
        symbolTable.back()[name] = symbol;
    }

    std::shared_ptr<Symbol> getSymbol(const std::string &name)
    {
        for (int i = symbolTable.size() - 1; i >= 0; --i)
        {
            auto it = symbolTable[i].find(name);
            if (it != symbolTable[i].end())
            {
                return it->second;
            }
        }
        return nullptr;
    }
};

#endif