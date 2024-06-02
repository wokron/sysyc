#include "ast.h"
#include "utils.h"
#include <functional>

template <typename T>
void _print_attr(std::ostream &out, std::string name, T value) {
    out << "\"" << name << "\"" << ":";

    if constexpr (std::is_same_v<T, std::string>) {
        out << "\"" << value << "\"";
    } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
        out << "null";
    } else {
        out << value;
    }
}

template <typename T>
using print_value_func_t = std::function<void(std::ostream &, const T &)>;

template <typename T>
void _print_attr(std::ostream &out, std::string name, T value,
                 print_value_func_t<T> print_value_func) {
    out << "\"" << name << "\"" << ":";
    print_value_func(out, value);
}

template <typename T>
void _print_items(std::ostream &out, const Items<T> &items,
                  print_value_func_t<T> print_item_func) {
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
void _print_items_attr(std::ostream &out, std::string name,
                       const Items<T> &items,
                       print_value_func_t<T> print_value_func) {
    out << "\"" << name << "\"" << ":";
    _print_items(out, items, print_value_func);
}

void _print_init_val(std::ostream &out, const InitVal &init_val);

void _print_array_init_val(std::ostream &out,
                           const ArrayInitVal &array_init_val) {
    out << "{";

    _print_items_attr<InitVal>(out, "items", array_init_val.items,
                               _print_init_val);
    out << "}";
}

void _print_exp(std::ostream &out, const Exp &exp);

void _print_init_val(std::ostream &out, const InitVal &init_val) {
    std::visit(overloaded{
                   [&out](const Exp &exp) { _print_exp(out, exp); },
                   [&out](const ArrayInitVal &array_init_val) {
                       _print_array_init_val(out, array_init_val);
                   },
               },
               init_val);
}

void _print_var_def(std::ostream &out, const VarDef &var_def) {
    out << "{";

    _print_attr(out, "ident", var_def.ident);
    out << ",";

    _print_items_attr<Exp>(out, "dims", *var_def.dims, _print_exp);
    out << ",";

    if (var_def.init_val == nullptr) {
        _print_attr(out, "init_val", nullptr);
    } else {
        _print_attr<InitVal>(out, "init_val", *var_def.init_val,
                             _print_init_val);
    }
    out << "}";
}

void _print_decl(std::ostream &out, const Decl &decl) {
    out << "{";

    _print_attr(out, "type", decl.type);
    out << ",";

    _print_attr(out, "btype", decl.btype);
    out << ",";

    _print_items_attr<VarDef>(out, "var_defs", *decl.var_defs, _print_var_def);
    out << "}";
}

void _print_lval(std::ostream &out, const LVal &lval) {
    std::visit(
        overloaded{[&out](const Ident &ident) { out << "\"" << ident << "\""; },
                   [&out](const Index &index) {
                       out << "{";

                       _print_attr<LVal>(out, "lval", *index.lval, _print_lval);
                       out << ",";

                       _print_attr<Exp>(out, "exp", *index.exp, _print_exp);
                       out << "}";
                   }},
        lval);
}

void _print_block_item(std::ostream &out, const BlockItem &block_item);
void _print_cond(std::ostream &out, const Cond &cond);

void _print_stmt(std::ostream &out, const Stmt &stmt) {
    std::visit(overloaded{
                   [&out](const AssignStmt &stmt) {
                       out << "{";

                       _print_attr<LVal>(out, "lval", *stmt.lval, _print_lval);
                       out << ",";

                       _print_attr<Exp>(out, "exp", *stmt.exp, _print_exp);
                       out << "}";
                   },
                   [&out](const ExpStmt &stmt) {
                       out << "{";

                       if (stmt.exp == nullptr) {
                           _print_attr(out, "exp", nullptr);
                       } else {
                           _print_attr<Exp>(out, "exp", *stmt.exp, _print_exp);
                       }
                       out << "}";
                   },
                   [&out](const BlockStmt &stmt) {
                       out << "{";

                       _print_items_attr<BlockItem>(out, "block", *stmt.block,
                                                    _print_block_item);
                       out << "}";
                   },
                   [&out](const IfStmt &stmt) {
                       out << "{";

                       _print_attr<Cond>(out, "cond", *stmt.cond, _print_cond);
                       out << ",";

                       _print_attr<Stmt>(out, "if_stmt", *stmt.if_stmt,
                                         _print_stmt);
                       out << ",";

                       if (stmt.else_stmt == nullptr) {
                           _print_attr(out, "else_stmt", nullptr);
                       } else {
                           _print_attr<Stmt>(out, "else_stmt", *stmt.else_stmt,
                                             _print_stmt);
                       }
                       out << "}";
                   },
                   [&out](const WhileStmt &stmt) {
                       out << "{";

                       _print_attr<Cond>(out, "cond", *stmt.cond, _print_cond);
                       out << ",";

                       _print_attr<Stmt>(out, "stmt", *stmt.stmt, _print_stmt);
                       out << "}";
                   },
                   [&out](const ControlStmt &stmt) {
                       out << "{";

                       _print_attr(out, "type", stmt.type);
                       out << "}";
                   },
                   [&out](const ReturnStmt &stmt) {
                       out << "{";

                       if (stmt.exp == nullptr) {
                           _print_attr(out, "exp", nullptr);
                       } else {
                           _print_attr<Exp>(out, "exp", *stmt.exp, _print_exp);
                       }
                       out << "}";
                   },
               },
               stmt);
}

void _print_exp(std::ostream &out, const Exp &exp) {
    std::visit(
        overloaded{
            [&out](const BinaryExp &exp) {
                out << "{";

                _print_attr<Exp>(out, "left", *exp.left, _print_exp);
                out << ",";

                _print_attr<Exp>(out, "right", *exp.right, _print_exp);
                out << ",";

                _print_attr(out, "binary_op", exp.op);
                out << "}";
            },
            [&out](const LValExp &lval) { _print_lval(out, *lval.lval); },
            [&out](const CallExp &exp) {
                out << "{";

                _print_attr(out, "ident", exp.ident);
                out << ",";

                _print_items_attr<Exp>(out, "func_rparams", *exp.func_rparams,
                                       _print_exp);
                out << "}";
            },
            [&out](const UnaryExp &exp) {
                out << "{";

                _print_attr<Exp>(out, "exp", *exp.exp, _print_exp);
                out << ",";

                _print_attr(out, "unary_op", exp.op);
                out << "}";
            },
            [&out](const CompareExp &exp) {
                out << "{";

                _print_attr<Exp>(out, "left", *exp.left, _print_exp);
                out << ",";

                _print_attr<Exp>(out, "right", *exp.right, _print_exp);
                out << ",";

                _print_attr(out, "compare_op", exp.op);
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

void _print_cond(std::ostream &out, const Cond &cond) {
    std::visit(overloaded{
                   [&out](const Exp &exp) { _print_exp(out, exp); },
                   [&out](const LogicalExp &exp) {
                       out << "{";

                       _print_attr<Cond>(out, "left", *exp.left, _print_cond);
                       out << ",";

                       _print_attr<Cond>(out, "right", *exp.right, _print_cond);
                       out << ",";

                       _print_attr(out, "logical_op", exp.op);
                       out << "}";
                   },
               },
               cond);
}

void _print_func_fparam(std::ostream &out, const FuncFParam &func_fparam) {
    out << "{";

    _print_attr(out, "btype", func_fparam.btype);
    out << ",";

    _print_attr(out, "ident", func_fparam.ident);
    out << ",";

    _print_items_attr<Exp>(out, "dims", *func_fparam.dims, _print_exp);
    out << "}";
}

void _print_block_item(std::ostream &out, const BlockItem &block_item) {
    std::visit(overloaded{
                   [&out](const Decl &decl) { _print_decl(out, decl); },
                   [&out](const Stmt &stmt) { _print_stmt(out, stmt); },
               },
               block_item);
}

void _print_func_def(std::ostream &out, const FuncDef &func_def) {
    out << "{";

    _print_attr(out, "btype", func_def.func_type);
    out << ",";

    _print_attr(out, "ident", func_def.ident);
    out << ",";

    _print_items_attr<FuncFParam>(out, "func_fparams", *func_def.func_fparams,
                                  _print_func_fparam);
    out << ",";

    _print_items_attr<BlockItem>(out, "block", *func_def.block,
                                 _print_block_item);
    out << "}";
}

void _print_comp_unit(std::ostream &out, const CompUnit &comp_unit) {
    std::visit(overloaded{[&out](const Decl &decl) { _print_decl(out, decl); },
                          [&out](const FuncDef &func_def) {
                              _print_func_def(out, func_def);
                          }},
               comp_unit);
}

void print_ast(std::ostream &out, const CompUnits &comp_units) {
    out << "{";

    _print_items_attr<CompUnit>(out, "comp_units", comp_units,
                                _print_comp_unit);
    out << "}";
}
