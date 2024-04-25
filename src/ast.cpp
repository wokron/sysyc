#include "ast.h"

#define PRINT_CHECK_NULL(attr)                                                 \
    if (attr != nullptr)                                                       \
        attr->print(out);                                                      \
    else                                                                       \
        out << nullptr;

#define PRINT_ATTR(name, attr)                                                 \
    out << "\"" #name "\": ";                                                  \
    PRINT_CHECK_NULL(attr);

#define PRINT_VAL(name, val) out << "\"" #name "\": " << val;

#define PRINT_ARRAY                                                            \
    out << "[";                                                                \
    for (auto item : items) {                                                  \
        if (item != items[0]) {                                                \
            out << ",";                                                        \
        }                                                                      \
        item->print(out);                                                      \
    }                                                                          \
    out << "]";

FuncRParams::~FuncRParams() {
    for (auto item : items) {
        delete item;
    }
}

void FuncRParams::print(std::ostream &out) { PRINT_ARRAY; }

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

void LVal::print(std::ostream &out) {
    out << "{";
    if (tag == IDENT) {
        auto d = data.IDENT;
        PRINT_VAL(ident, d);
    } else {
        auto d = data.INDEX;
        PRINT_ATTR(exp, d.exp);
        out << ",";
        PRINT_ATTR(lval, d.lval);
    }
    out << "}";
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

void Exp::print(std::ostream &out) {
    out << "{";
    switch (tag) {
    case BINARY: {
        auto d = data.BINARY;
        PRINT_ATTR(left, d.left);
        out << ",";
        PRINT_VAL(op, d.op);
        out << ",";
        PRINT_ATTR(right, d.right);
    } break;
    case BOOL: {
        auto d = data.BOOL;
        PRINT_ATTR(left, d.left);
        out << ",";
        PRINT_VAL(op, d.op);
        out << ",";
        PRINT_ATTR(right, d.right);
    } break;
    case LVAL: {
        auto d = data.LVAL;
        PRINT_ATTR(lval, d);
    } break;
    case CALL: {
        auto d = data.CALL;
        PRINT_VAL(ident, d.ident);
        out << ",";
        PRINT_ATTR(func_rparams, d.func_rparams);
    } break;
    case UNARY: {
        auto d = data.UNARY;
        PRINT_VAL(op, d.op);
        out << ",";
        PRINT_ATTR(exp, d.exp);
    } break;
    case COMPARE: {
        auto d = data.COMPARE;
        PRINT_ATTR(left, d.left);
        out << ",";
        PRINT_VAL(op, d.op);
        out << ",";
        PRINT_ATTR(right, d.right);
    } break;
    case NUMBER: {
        auto d = data.NUMBER;
        PRINT_VAL(type, d.type);
        out << ",";
        out << (d.type == ASTType::INT ? d.val.int_val : d.val.float_val);
    }
    }
    out << "}";
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

void Stmt::print(std::ostream &out) {
    out << "{";
    switch (tag) {
    case ASSIGN: {
        auto d = data.ASSIGN;
        PRINT_ATTR(exp, d.exp);
        out << ",";
        PRINT_ATTR(lval, d.lval);
    } break;
    case EXP: {
        auto d = data.EXP;
        PRINT_ATTR(exp, d);
    } break;
    case BLOCK: {
        auto d = data.BLOCK;
        PRINT_ATTR(block, d);
    } break;
    case IF: {
        auto d = data.IF;
        PRINT_ATTR(if_cond, d.cond);
        out << ",";
        PRINT_ATTR(if_stmt, d.if_stmt);
        out << ",";
        PRINT_ATTR(else_stmt, d.else_stmt);
    } break;
    case WHILE: {
        auto d = data.WHILE;
        PRINT_ATTR(while_cond, d.cond);
        out << ",";
        PRINT_ATTR(stmt, d.stmt);
    } break;
    case CONTINUE:
        PRINT_VAL(control, "continue");
        break;
    case BREAK:
        PRINT_VAL(control, "break");
        break;
    case RETURN: {
        auto d = data.RETURN;
        PRINT_ATTR(return, d);
    } break;
    }
    out << "}";
}

ArrayInitVal::~ArrayInitVal() {
    for (auto item : items) {
        delete item;
    }
}

void ArrayInitVal::print(std::ostream &out) { PRINT_ARRAY; }

InitVal::~InitVal() {
    if (tag == EXP) {
        auto d = data.EXP;
        delete d;
    } else {
        auto d = data.ARRAY;
        delete d;
    }
}

void InitVal::print(std::ostream &out) {
    out << "{";
    if (tag == EXP) {
        auto d = data.EXP;
        PRINT_ATTR(exp, d);
    } else {
        auto d = data.ARRAY;
        PRINT_ATTR(init_vals, d);
    }
    out << "}";
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

void BlockItem::print(std::ostream &out) {
    out << "{";
    if (tag == DECL) {
        auto d = data.DECL;
        PRINT_ATTR(decl, d);
    } else {
        auto d = data.STMT;
        PRINT_ATTR(stmt, d);
    }
    out << "}";
}

CompUnit::~CompUnit() {
    if (tag == DECL) {
        auto d = data.DECL;
        delete d;
    } else {
        auto d = data.FUNC;
        delete d;
    }
}

void CompUnit::print(std::ostream &out) {
    out << "{";
    if (tag == DECL) {
        auto d = data.DECL;
        PRINT_ATTR(decl, d);
    } else {
        auto d = data.FUNC;
        PRINT_ATTR(func_def, d);
    }
    out << "}";
}

Dims::~Dims() {
    for (auto item : items) {
        delete item;
    }
}

void Dims::print(std::ostream &out) { PRINT_ARRAY; }

void VarDef::print(std::ostream &out) {
    out << "{";
    PRINT_VAL(ident, ident);
    out << ",";
    PRINT_ATTR(dims, dims);
    out << ",";
    PRINT_ATTR(init_val, init_val);
    out << "}";
}

VarDefs::~VarDefs() {
    for (auto item : items) {
        delete item;
    }
}

void VarDefs::print(std::ostream &out) { PRINT_ARRAY; }

void Decl::print(std::ostream &out) {
    out << "{";
    PRINT_VAL(var_type, var_type);
    out << ",";
    PRINT_VAL(btype, btype);
    out << ",";
    PRINT_ATTR(var_defs, var_defs);
    out << "}";
}

BlockItems::~BlockItems() {
    for (auto item : items) {
        delete item;
    }
}

void BlockItems::print(std::ostream &out) { PRINT_ARRAY; }

void FuncFParam::print(std::ostream &out) {
    out << "{";
    PRINT_VAL(btype, btype);
    out << ",";
    PRINT_VAL(ident, ident);
    out << ",";
    PRINT_ATTR(dims, dims);
    out << "}";
}

FuncFParams::~FuncFParams() {
    for (auto item : items) {
        delete item;
    }
}
void FuncFParams::print(std::ostream &out) { PRINT_ARRAY; }

void FuncDef::print(std::ostream &out) {
    out << "{";
    PRINT_VAL(func_type, func_type);
    out << ",";
    PRINT_VAL(ident, ident);
    out << ",";
    PRINT_ATTR(func_fparams, func_fparams);
    out << ",";
    PRINT_ATTR(block, block);
    out << "}";
}

CompUnits::~CompUnits() {
    for (auto item : items) {
        delete item;
    }
}

void CompUnits::print(std::ostream &out) { PRINT_ARRAY; }
