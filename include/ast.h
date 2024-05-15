#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <ostream>

enum ASTType {
    INT,
    FLOAT,
    VOID,
};

template <typename T> using Items = std::vector<std::shared_ptr<T>>;

using Ident = std::string;
using Number = std::variant<int, float>;

struct Index;
using LVal = std::variant<Ident, Index>;

struct BinaryExp;
struct CallExp;
struct UnaryExp;
struct CompareExp;
// TODO: LvalExp is needed, instead of LVal
using Exp = std::variant<BinaryExp, LVal, CallExp, UnaryExp,
                         CompareExp, Number>;

struct LogicalExp;
using Cond = std::variant<Exp, LogicalExp>;

using FuncRParams = Items<Exp>;

struct AssignStmt;
struct ExpStmt;
struct BlockStmt;
struct IfStmt;
struct WhileStmt;
struct ControlStmt;
struct ReturnStmt;
using Stmt = std::variant<AssignStmt, ExpStmt, BlockStmt, IfStmt, WhileStmt,
                          ControlStmt, ReturnStmt>;

struct ArrayInitVal;
using InitVal = std::variant<Exp, ArrayInitVal>;

using Dims = Items<Exp>; // nullable

struct VarDef;
using VarDefs = Items<VarDef>;

struct FuncFParam;
using FuncFParams = Items<FuncFParam>;

struct Decl;
using BlockItem = std::variant<Decl, Stmt>;
using BlockItems = Items<BlockItem>;

struct FuncDef;
using CompUnit = std::variant<Decl, FuncDef>;

using CompUnits = Items<CompUnit>;

struct Index {
    std::shared_ptr<LVal> lval;
    std::shared_ptr<Exp> exp;
};

struct BinaryExp {
    std::shared_ptr<Exp> left;
    enum {
        ADD,
        SUB,
        MULT,
        DIV,
        MOD,
    } op;
    std::shared_ptr<Exp> right;
};

struct LogicalExp {
    std::shared_ptr<Cond> left;
    enum {
        AND,
        OR,
    } op;
    std::shared_ptr<Cond> right;
};

struct CallExp {
    Ident ident;
    std::shared_ptr<FuncRParams> func_rparams;
};

struct UnaryExp {
    enum {
        NOT,
        ADD,
        SUB,
    } op;
    std::shared_ptr<Exp> exp;
};

struct CompareExp {
    std::shared_ptr<Exp> left;
    enum {
        EQ,
        NE,
        LT,
        LE,
        GT,
        GE,
    } op;
    std::shared_ptr<Exp> right;
};

struct AssignStmt {
    std::shared_ptr<LVal> lval;
    std::shared_ptr<Exp> exp;
};

struct ExpStmt {
    std::shared_ptr<Exp> exp; // nullable
};

struct BlockStmt {
    std::shared_ptr<BlockItems> block;
};

struct IfStmt {
    std::shared_ptr<Cond> cond;
    std::shared_ptr<Stmt> if_stmt;
    std::shared_ptr<Stmt> else_stmt; // nullable
};

struct WhileStmt {
    std::shared_ptr<Cond> cond;
    std::shared_ptr<Stmt> stmt;
};

struct ControlStmt {
    enum {
        BREAK,
        CONTINUE,
    } type;
};

struct ReturnStmt {
    std::shared_ptr<Exp> exp; // nullable
};

struct ArrayInitVal {
    std::vector<std::shared_ptr<InitVal>> items;
};

struct VarDef {
    Ident ident;
    std::shared_ptr<Dims> dims;
    std::shared_ptr<InitVal> init_val; // nullable
};

struct Decl {
    enum {
        CONST,
        VAR,
    } type;

    ASTType btype;
    std::shared_ptr<VarDefs> var_defs;
};

struct FuncFParam {
    ASTType btype;
    Ident ident;
    std::shared_ptr<Dims> dims;
};

struct FuncDef {
    ASTType func_type;
    Ident ident;
    std::shared_ptr<FuncFParams> func_fparams;
    std::shared_ptr<BlockItems> block;
};

void print_ast(std::ostream &out, const CompUnits &comp_units);
