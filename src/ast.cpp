#include "ast.h"
#include "utils.h"
#include <functional>

template <typename T>
static void print_attr(std::ostream &out, std::string name, T value) {
    out << "\"" << name << "\""
        << ":";

    if constexpr (std::is_same_v<T, std::string>) {
        out << "\"" << value << "\"";
    } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
        out << "null";
    } else {
        out << value;
    }
}

template <typename T>
using PrintValueFunc = std::function<void(std::ostream &, const T &)>;

template <typename T>
static void print_attr(std::ostream &out, std::string name, T value,
                       PrintValueFunc<T> print_value_func) {
    out << "\"" << name << "\""
        << ":";
    print_value_func(out, value);
}

template <typename T>
static void print_items(std::ostream &out, const Items<T> &items,
                        PrintValueFunc<T> print_item_func) {
    out << "[";
    for (const auto &item : items) {
        if (&item != &items[0]) {
            out << ",";
        }
        if (item == nullptr) {
            out << "null";
        } else {
            print_item_func(out, *item);
        }
    }
    out << "]";
}

template <typename T>
static void print_items_attr(std::ostream &out, std::string name,
                             const Items<T> &items,
                             PrintValueFunc<T> print_value_func) {
    out << "\"" << name << "\""
        << ":";
    print_items(out, items, print_value_func);
}

static void print_init_val(std::ostream &out, const InitVal &init_val);

static void print_array_init_val(std::ostream &out,
                                 const ArrayInitVal &array_init_val) {
    out << "{";

    print_items_attr<InitVal>(out, "items", array_init_val.items,
                              print_init_val);
    out << "}";
}

static void print_exp(std::ostream &out, const Exp &exp);

static void print_init_val(std::ostream &out, const InitVal &init_val) {
    std::visit(overloaded{
                   [&out](const Exp &exp) { print_exp(out, exp); },
                   [&out](const ArrayInitVal &array_init_val) {
                       print_array_init_val(out, array_init_val);
                   },
               },
               init_val);
}

static void print_var_def(std::ostream &out, const VarDef &var_def) {
    out << "{";

    print_attr(out, "ident", var_def.ident);
    out << ",";

    print_items_attr<Exp>(out, "dims", *var_def.dims, print_exp);
    out << ",";

    if (var_def.init_val == nullptr) {
        print_attr(out, "init_val", nullptr);
    } else {
        print_attr<InitVal>(out, "init_val", *var_def.init_val, print_init_val);
    }
    out << "}";
}

static void print_decl(std::ostream &out, const Decl &decl) {
    out << "{";

    print_attr(out, "type", decl.type);
    out << ",";

    print_attr(out, "btype", decl.btype);
    out << ",";

    print_items_attr<VarDef>(out, "var_defs", *decl.var_defs, print_var_def);
    out << "}";
}

static void print_lval(std::ostream &out, const LVal &lval) {
    std::visit(
        overloaded{[&out](const Ident &ident) { out << "\"" << ident << "\""; },
                   [&out](const Index &index) {
                       out << "{";

                       print_attr<LVal>(out, "lval", *index.lval, print_lval);
                       out << ",";

                       print_attr<Exp>(out, "exp", *index.exp, print_exp);
                       out << "}";
                   }},
        lval);
}

static void print_block_item(std::ostream &out, const BlockItem &block_item);
static void print_cond(std::ostream &out, const Cond &cond);

static void print_stmt(std::ostream &out, const Stmt &stmt) {
    std::visit(overloaded{
                   [&out](const AssignStmt &stmt) {
                       out << "{";

                       print_attr<LVal>(out, "lval", *stmt.lval, print_lval);
                       out << ",";

                       print_attr<Exp>(out, "exp", *stmt.exp, print_exp);
                       out << "}";
                   },
                   [&out](const ExpStmt &stmt) {
                       out << "{";

                       if (stmt.exp == nullptr) {
                           print_attr(out, "exp", nullptr);
                       } else {
                           print_attr<Exp>(out, "exp", *stmt.exp, print_exp);
                       }
                       out << "}";
                   },
                   [&out](const BlockStmt &stmt) {
                       out << "{";

                       print_items_attr<BlockItem>(out, "block", *stmt.block,
                                                   print_block_item);
                       out << "}";
                   },
                   [&out](const IfStmt &stmt) {
                       out << "{";

                       print_attr<Cond>(out, "cond", *stmt.cond, print_cond);
                       out << ",";

                       print_attr<Stmt>(out, "if_stmt", *stmt.if_stmt,
                                        print_stmt);
                       out << ",";

                       if (stmt.else_stmt == nullptr) {
                           print_attr(out, "else_stmt", nullptr);
                       } else {
                           print_attr<Stmt>(out, "else_stmt", *stmt.else_stmt,
                                            print_stmt);
                       }
                       out << "}";
                   },
                   [&out](const WhileStmt &stmt) {
                       out << "{";

                       print_attr<Cond>(out, "cond", *stmt.cond, print_cond);
                       out << ",";

                       print_attr<Stmt>(out, "stmt", *stmt.stmt, print_stmt);
                       out << "}";
                   },
                   [&out](const ControlStmt &stmt) {
                       out << "{";

                       print_attr(out, "type", stmt.type);
                       out << "}";
                   },
                   [&out](const ReturnStmt &stmt) {
                       out << "{";

                       if (stmt.exp == nullptr) {
                           print_attr(out, "exp", nullptr);
                       } else {
                           print_attr<Exp>(out, "exp", *stmt.exp, print_exp);
                       }
                       out << "}";
                   },
               },
               stmt);
}

static void print_exp(std::ostream &out, const Exp &exp) {
    std::visit(
        overloaded{
            [&out](const BinaryExp &exp) {
                out << "{";

                print_attr<Exp>(out, "left", *exp.left, print_exp);
                out << ",";

                print_attr<Exp>(out, "right", *exp.right, print_exp);
                out << ",";

                print_attr(out, "binary_op", exp.op);
                out << "}";
            },
            [&out](const LValExp &lval) { print_lval(out, *lval.lval); },
            [&out](const CallExp &exp) {
                out << "{";

                print_attr(out, "ident", exp.ident);
                out << ",";

                print_items_attr<Exp>(out, "func_rparams", *exp.func_rparams,
                                      print_exp);
                out << "}";
            },
            [&out](const UnaryExp &exp) {
                out << "{";

                print_attr<Exp>(out, "exp", *exp.exp, print_exp);
                out << ",";

                print_attr(out, "unary_op", exp.op);
                out << "}";
            },
            [&out](const CompareExp &exp) {
                out << "{";

                print_attr<Exp>(out, "left", *exp.left, print_exp);
                out << ",";

                print_attr<Exp>(out, "right", *exp.right, print_exp);
                out << ",";

                print_attr(out, "compare_op", exp.op);
                out << "}";
            },
            [&out](const Number &number) {
                std::visit(
                    overloaded{[&out](const int number) { out << number; },
                               [&out](const float number) { out << number; }},
                    number);
            },
        },
        exp);
}

static void print_cond(std::ostream &out, const Cond &cond) {
    std::visit(overloaded{
                   [&out](const Exp &exp) { print_exp(out, exp); },
                   [&out](const LogicalExp &exp) {
                       out << "{";

                       print_attr<Cond>(out, "left", *exp.left, print_cond);
                       out << ",";

                       print_attr<Cond>(out, "right", *exp.right, print_cond);
                       out << ",";

                       print_attr(out, "logical_op", exp.op);
                       out << "}";
                   },
               },
               cond);
}

static void print_func_fparam(std::ostream &out,
                              const FuncFParam &func_fparam) {
    out << "{";

    print_attr(out, "btype", func_fparam.btype);
    out << ",";

    print_attr(out, "ident", func_fparam.ident);
    out << ",";

    print_items_attr<Exp>(out, "dims", *func_fparam.dims, print_exp);
    out << "}";
}

static void print_block_item(std::ostream &out, const BlockItem &block_item) {
    std::visit(overloaded{
                   [&out](const Decl &decl) { print_decl(out, decl); },
                   [&out](const Stmt &stmt) { print_stmt(out, stmt); },
               },
               block_item);
}

static void print_func_def(std::ostream &out, const FuncDef &func_def) {
    out << "{";

    print_attr(out, "btype", func_def.func_type);
    out << ",";

    print_attr(out, "ident", func_def.ident);
    out << ",";

    print_items_attr<FuncFParam>(out, "func_fparams", *func_def.func_fparams,
                                 print_func_fparam);
    out << ",";

    print_items_attr<BlockItem>(out, "block", *func_def.block,
                                print_block_item);
    out << "}";
}

static void print_comp_unit(std::ostream &out, const CompUnit &comp_unit) {
    std::visit(overloaded{[&out](const Decl &decl) { print_decl(out, decl); },
                          [&out](const FuncDef &func_def) {
                              print_func_def(out, func_def);
                          }},
               comp_unit);
}

void print_ast(std::ostream &out, const CompUnits &comp_units) {
    out << "{";

    print_items_attr<CompUnit>(out, "comp_units", comp_units, print_comp_unit);
    out << "}";
}
