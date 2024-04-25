#pragma once

#include <cstdlib>
#include <iostream>
#include <vector>

enum ASTType {
    INT,
    FLOAT,
    VOID,
};

struct Exp;

struct FuncRParams {
    std::vector<Exp *> items;

    ~FuncRParams();
    void print(std::ostream &out);
};

struct LVal {
    enum {
        IDENT,
        INDEX,
    } tag;

    union Data {
        char *IDENT;

        struct {
            LVal *lval;
            Exp *exp;
        } INDEX;
    } data;

    ~LVal();
    void print(std::ostream &out);
};

struct Exp {
    enum {
        BINARY,
        BOOL,
        LVAL,
        CALL,
        UNARY,
        COMPARE,
        NUMBER,
    } tag;

    union Data {
        struct BinaryExp {
            Exp *left;
            enum {
                ADD,
                SUB,
                MULT,
                DIV,
                MOD,
            } op;
            Exp *right;
        } BINARY;

        struct BoolExp {
            Exp *left;
            enum {
                AND,
                OR,
            } op;
            Exp *right;
        } BOOL;

        LVal *LVAL;

        struct {
            char *ident;
            FuncRParams *func_rparams;
        } CALL;

        struct UnaryExp {
            enum {
                NOT,
                ADD,
                SUB,
            } op;
            Exp *exp;
        } UNARY;

        struct Compare {
            Exp *left;
            enum {
                EQ,
                NE,
                LT,
                LE,
                GT,
                GE,
            } op;
            Exp *right;
        } COMPARE;

        struct {
            ASTType type;
            union {
                int int_val;
                float float_val;
            } val;
        } NUMBER;
    } data;

    ~Exp();
    void print(std::ostream &out);
};

struct BlockItems;

struct Stmt {
    enum {
        ASSIGN,
        EXP,
        BLOCK,
        IF,
        WHILE,
        CONTINUE,
        BREAK,
        RETURN,
    } tag;

    union Data {
        struct {
            LVal *lval;
            Exp *exp;
        } ASSIGN;
        Exp *EXP;

        BlockItems *BLOCK;

        struct {
            Exp *cond;
            Stmt *if_stmt;
            Stmt *else_stmt;
        } IF;

        struct {
            Exp *cond;
            Stmt *stmt;
        } WHILE;

        Exp *RETURN;
    } data;

    ~Stmt();
    void print(std::ostream &out);
};

struct InitVal;

struct ArrayInitVal {
    std::vector<InitVal *> items;

    ~ArrayInitVal();
    void print(std::ostream &out);
};

struct InitVal {
    enum {
        EXP,
        ARRAY,
    } tag;

    union Data {
        Exp *EXP;

        ArrayInitVal *ARRAY;
    } data;

    ~InitVal();
    void print(std::ostream &out);
};

struct Dims {
    std::vector<Exp *> items;

    ~Dims();
    void print(std::ostream &out);
};

struct VarDef {
    char *ident;
    Dims *dims;
    InitVal *init_val;

    ~VarDef() {
        free(ident);
        delete dims;
        delete init_val;
    }
    void print(std::ostream &out);
};

struct VarDefs {
    std::vector<VarDef *> items;

    ~VarDefs();
    void print(std::ostream &out);
};

struct Decl {
    enum {
        CONST,
        VAR,
    } var_type;

    ASTType btype;
    VarDefs *var_defs;

    ~Decl() { delete var_defs; }
    void print(std::ostream &out);
};

struct BlockItem {
    enum {
        DECL,
        STMT,
    } tag;
    union Data {
        Decl *DECL;
        Stmt *STMT;
    } data;

    ~BlockItem();
    void print(std::ostream &out);
};

struct BlockItems {
    std::vector<BlockItem *> items;

    ~BlockItems();
    void print(std::ostream &out);
};

struct FuncFParam {
    ASTType btype;
    char *ident;
    Dims *dims;

    ~FuncFParam() {
        free(ident);
        delete dims;
    }
    void print(std::ostream &out);
};

struct FuncFParams {
    std::vector<FuncFParam *> items;

    ~FuncFParams();
    void print(std::ostream &out);
};

struct FuncDef {
    ASTType func_type;
    char *ident;
    FuncFParams *func_fparams;
    BlockItems *block;

    ~FuncDef() {
        free(ident);
        delete func_fparams;
        delete block;
    }
    void print(std::ostream &out);
};

struct CompUnit {
    enum {
        DECL,
        FUNC,
    } tag;

    union Data {
        Decl *DECL;

        FuncDef *FUNC;
    } data;

    ~CompUnit();
    void print(std::ostream &out);
};

struct CompUnits {
    std::vector<CompUnit *> items;

    ~CompUnits();
    void print(std::ostream &out);
};

/**
 * some macro magic... this macro is used to construct AST node in an uniform
 * way
 */
#define AST(cls, tag, ...)                                                     \
    (cls) {                                                                    \
        cls ::tag, { .tag = __VA_ARGS__ }                                      \
    }
