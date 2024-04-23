#pragma once

#include <cstdlib>

enum ASTType {
    INT,
    FLOAT,
    VOID,
};

struct Exp;

struct FuncRParams {
    FuncRParams *func_rparams;
    Exp *exp;

    ~FuncRParams();
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
};

struct InitVal {
    enum {
        EXP,
        ARRAY,
    } tag;

    union Data {
        Exp *EXP;

        struct {
            InitVal *init_vals;
            InitVal *init_val;
        } ARRAY;
    } data;

    ~InitVal();
};

struct Dims {
    Dims *dims;
    Exp *const_exp;

    ~Dims() {
        delete dims;
        delete const_exp;
    }
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
};

struct VarDefs {
    VarDefs *var_defs;
    VarDef *var_def;

    ~VarDefs() {
        delete var_defs;
        delete var_def;
    }
};

struct Decl {
    enum {
        CONST,
        VAR,
    } var_type;

    ASTType btype;
    VarDefs *var_defs;

    ~Decl() { delete var_defs; }
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
};

struct BlockItems {
    BlockItems *block_items;
    BlockItem *block_item;

    ~BlockItems() {
        delete block_items;
        delete block_item;
    }
};

struct FuncFParam {
    ASTType btype;
    char *ident;
    Dims *dims;

    ~FuncFParam() {
        free(ident);
        delete dims;
    }
};

struct FuncFParams {
    FuncFParams *func_fparams;
    FuncFParam *func_fparam;

    ~FuncFParams() {
        delete func_fparams;
        delete func_fparam;
    }
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
};

struct CompUnit {
    enum {
        DECL,
        FUNC,
    } tag;

    union Data {
        struct {
            CompUnit *comp_unit;
            Decl *decl;
        } DECL;

        struct {
            CompUnit *comp_unit;
            FuncDef *func_def;
        } FUNC;
    } data;

    ~CompUnit();
};

/**
 * some macro magic... this macro is used to construct AST node in an uniform
 * way
 */
#define AST(cls, tag, ...)                                                     \
    (cls) {                                                                    \
        cls ::tag, { .tag = __VA_ARGS__ }                                      \
    }
