#include "ast.h"

FuncRParams::~FuncRParams() {
    delete func_rparams;
    delete exp;
}

LVal::~LVal() {
    if (tag == IDENT) {
        auto d = data.IDENT;
        delete d;
    } else {
        auto d = data.INDEX;
        delete d.exp;
        delete d.lval;
    }
}

Exp::~Exp() {
    switch (tag) {
    case BINARY: {
        auto d = data.BINARY;
        delete d.left;
        delete d.right;
    } break;
    case BOOL: {
        auto d = data.BOOL;
        delete d.left;
        delete d.right;
    } break;
    case LVAL: {
        auto d = data.LVAL;
        delete d;
    } break;
    case CALL: {
        auto d = data.CALL;
        free(d.ident);
        delete d.func_rparams;
    } break;
    case UNARY: {
        auto d = data.UNARY;
        delete d.exp;
    } break;
    case COMPARE: {
        auto d = data.COMPARE;
        delete d.left;
        delete d.right;
    } break;
    }
}

Stmt::~Stmt() {
    switch (tag) {
    case ASSIGN: {
        auto d = data.ASSIGN;
        delete d.exp;
        delete d.lval;
    } break;
    case EXP: {
        auto d = data.EXP;
        delete d;
    } break;
    case BLOCK: {
        auto d = data.BLOCK;
        delete d;
    } break;
    case IF: {
        auto d = data.IF;
        delete d.cond;
        delete d.if_stmt;
        delete d.else_stmt;
    } break;
    case WHILE: {
        auto d = data.WHILE;
        delete d.cond;
        delete d.stmt;
    } break;
    case CONTINUE:
        break;
    case BREAK:
        break;
    case RETURN: {
        auto d = data.RETURN;
        delete d;
    } break;
    }
}

InitVal::~InitVal() {
    if (tag == EXP) {
        auto d = data.EXP;
        delete d;
    } else {
        auto d = data.ARRAY;
        delete d.init_vals;
        delete d.init_val;
    }
}

BlockItem::~BlockItem() {
    if (tag == DECL) {
        auto d = data.DECL;
        delete d;
    } else {
        auto d = data.STMT;
        delete d;
    }
}

CompUnit::~CompUnit() {
    if (tag == DECL) {
        auto d = data.DECL;
        delete d.comp_unit;
        delete d.decl;
    } else {
        auto d = data.FUNC;
        delete d.comp_unit;
        delete d.func_def;
    }
}
