#include "ast.h"
#include "error.h"
#include "utils.h"
#include "visitor.h"
#include <variant>

sym::TypePtr Visitor::_asttype2symtype(ASTType type) {
    switch (type) {
    case ASTType::INT:
        return sym::Int32Type::get();
    case ASTType::FLOAT:
        return sym::FloatType::get();
    case ASTType::VOID:
        return sym::VoidType::get();
    default:
        throw std::logic_error("unreachable");
    }
}

ir::Type Visitor::_symtype2irtype(const sym::Type &type) {
    if (type.is_int32()) {
        return ir::Type::W;
    } else if (type.is_float()) {
        return ir::Type::S;
    } else if (type.is_array() || type.is_pointer()) {
        return ir::Type::L;
    } else {
        return ir::Type::X;
    }
}

void Visitor::visit(const CompUnits &node) {
    for (auto &elm : node) {
        std::visit(overloaded{
                       [this](const Decl &node) { visit_decl(node); },
                       [this](const FuncDef &node) { visit_func_def(node); },
                   },
                   *elm);
    }
}

void Visitor::visit_decl(const Decl &node) {
    for (auto &elm : *node.var_defs) {
        bool is_const = (node.type == Decl::CONST);
        visit_var_def(*elm, node.btype, is_const);
    }
}

ir::ConstBitsPtr Visitor::_convert_const(ir::Type target_type,
                                         const ir::ConstBits &const_val) {
    if (target_type == ir::Type::W) {
        return const_val.to_int();
    } else if (target_type == ir::Type::S) {
        return const_val.to_float();
    } else {
        return nullptr;
    }
}

void Visitor::_init_global(ir::Data &data, const sym::Type &elm_type,
                           const sym::Initializer &initializer) {
    // since we use map instead of unordered_map in Initializer, the order of
    // initial values is guaranteed
    int prev_index = -1;
    for (auto &[index, value] : initializer.get_values()) {
        auto &[val_type, val] = value;
        auto zero_count = index - prev_index - 1;
        prev_index = index;
        if (zero_count > 0) {
            data.append_zero(elm_type.get_size() * zero_count);
        }

        // init value should be constant if the variable is global
        auto const_val = std::dynamic_pointer_cast<ir::ConstBits>(val);

        if (!const_val) {
            error(-1, "init value must be constant for global variable");
            // just to make it work
            data.append_zero(elm_type.get_size());
            continue;
        }

        auto elm_ir_type = _symtype2irtype(elm_type);
        const_val = _convert_const(elm_ir_type, *const_val);
        if (!const_val) {
            error(-1, "unsupported type in global variable");
            // just to make it work
            data.append_zero(elm_type.get_size());
            continue;
        }

        data.append_const(elm_ir_type, {const_val});
    }

    // don't forget this
    auto zero_count = initializer.get_space() - prev_index - 1;
    if (zero_count > 0) {
        data.append_zero(elm_type.get_size() * zero_count);
    }
}

bool Visitor::_can_unroll_loop(const WhileStmt &node, int &from, int &to,
                               bool &is_mini_loop) {
    auto temp = std::get_if<Exp>(node.cond.get());
    if (!temp) {
        return false;
    }
    auto cmp_cond = std::get_if<CompareExp>(temp);
    if (cmp_cond == nullptr || cmp_cond->op != CompareExp::LT) {
        return false;
    }
    auto temp2 = std::get_if<LValExp>(cmp_cond->left.get());
    if (!temp2) {
        return false;
    }
    auto ident = std::get_if<Ident>(temp2->lval.get());
    if (ident == nullptr) {
        return false;
    }
    auto symbol = _current_scope->get_symbol(*ident);
    auto it = _last_store.find(symbol->value);
    if (it == _last_store.end()) {
        return false;
    }
    auto const_bits = std::dynamic_pointer_cast<ir::ConstBits>(it->second);
    if (const_bits == nullptr) {
        return false;
    }
    if (auto int_val = std::get_if<int>(&const_bits->value)) {
        from = *int_val;
    } else {
        return false;
    }

    auto temp3 = std::get_if<Number>(cmp_cond->right.get());
    if (!temp3) {
        return false;
    }
    auto max_val = std::get_if<int>(temp3);
    if (max_val != nullptr) {
        to = *max_val;
    } else {
        return false;
    }

    auto block_stmt = std::get_if<BlockStmt>(node.stmt.get());
    if (block_stmt == nullptr) {
        return false;
    }

    if (block_stmt->block->empty()) {
        return false;
    }

    is_mini_loop = block_stmt->block->size() < 4;

    auto temp4 = std::get_if<Stmt>(block_stmt->block->back().get());
    if (!temp4) {
        return false;
    }
    auto assign = std::get_if<AssignStmt>(temp4);
    if (assign == nullptr) {
        return false;
    }

    auto ident2 = std::get_if<Ident>(assign->lval.get());
    if (ident2 == nullptr) {
        return false;
    }

    auto add = std::get_if<BinaryExp>(assign->exp.get());
    if (add == nullptr || add->op != BinaryExp::ADD) {
        return false;
    }

    auto temp5 = std::get_if<LValExp>(add->left.get());
    if (!temp5) {
        return false;
    }
    auto ident3 = std::get_if<Ident>(temp5->lval.get());
    if (ident3 == nullptr) {
        return false;
    }

    auto temp6 = std::get_if<Number>(add->right.get());
    if (!temp6) {
        return false;
    }
    auto one = std::get_if<int>(temp6);
    if (one == nullptr || *one != 1) {
        return false;
    }

    if (!(*ident == *ident2 && *ident2 == *ident3)) {
        return false;
    }

    if (_has_control_stmt(*(node.stmt))) {
        return false;
    }

    return true;
}

bool Visitor::_has_control_stmt(const Stmt &node) {
    return std::visit(
        overloaded{
            [this](const AssignStmt &node) { return false; },
            [this](const ExpStmt &node) { return false; },
            [this](const BlockStmt &node) {
                for (auto block_item : *node.block) {
                    if (auto stmt_item = std::get_if<Stmt>(block_item.get());
                        stmt_item != nullptr && _has_control_stmt(*stmt_item)) {
                        return true;
                    }
                }
                return false;
            },
            [this](const IfStmt &node) {
                if (_has_control_stmt(*(node.if_stmt))) {
                    return true;
                }
                if (node.else_stmt != nullptr &&
                    _has_control_stmt(*(node.else_stmt))) {
                    return true;
                }
                return false;
            },
            [this](const WhileStmt &node) { return false; },
            [this](const ControlStmt &node) { return true; },
            [this](const ReturnStmt &node) { return false; },
        },
        node);
}

void Visitor::visit_var_def(const VarDef &node, ASTType btype, bool is_const) {
    auto type = visit_dims(*node.dims, btype);

    sym::InitializerPtr initializer = nullptr;
    if (node.init_val != nullptr) {
        // if is global, init value must be const as well, since we can't
        // generate instructions for it
        auto init_is_const = _is_global_context() || is_const;
        initializer = visit_init_val(*node.init_val, *type, init_is_const);
    }

    auto symbol = std::make_shared<sym::VariableSymbol>(node.ident, type,
                                                        is_const, initializer);

    if (!_current_scope->add_symbol(symbol)) {
        error(-1, "redefine variable " + node.ident);
    }

    if (_is_global_context()) {
        auto elm_type = _asttype2symtype(btype);
        auto data = ir::Data::create(false, symbol->name, elm_type->get_size(),
                                     _module);

        symbol->value = data->get_address();

        if (!symbol->initializer) {
            // still need to append zero if no initializer
            data->append_zero(type->get_size());
        } else {
            _init_global(*data, *elm_type, *initializer);
        }
    } else { // in a function
        auto elm_type = _asttype2symtype(btype);
        auto elm_ir_type = _symtype2irtype(*elm_type);

        symbol->value = _builder.create_alloc(elm_ir_type, type->get_size());

        if (symbol->initializer) {
            auto values = symbol->initializer->get_values();
            for (int index = 0; index < initializer->get_space(); index++) {
                // if no init value, just store zero
                sym::TypePtr val_type = elm_type;
                ir::ValuePtr val = val_type->is_float()
                                       ? ir::ConstBits::get(0.0f)
                                       : ir::ConstBits::get(0);
                if (values.find(index) != values.end()) {
                    auto value = values[index];
                    val_type = std::get<0>(value);
                    val = std::get<1>(value);
                }

                if (auto const_val =
                        std::dynamic_pointer_cast<ir::ConstBits>(val);
                    !const_val && is_const) {
                    error(-1, "init value must be constant for const variable");
                    continue;
                }

                auto offset = ir::ConstBits::get(elm_type->get_size() * index);
                auto elm_addr =
                    _builder.create_add(ir::Type::L, symbol->value, offset);
                val = _convert_if_needed(*elm_type, *val_type, val);
                _builder.create_store(elm_ir_type, val, elm_addr);
                _last_store[elm_addr] = val;
            }
        }
    }
}

sym::InitializerPtr Visitor::visit_init_val(const InitVal &node,
                                            sym::Type &type, bool is_const) {
    return std::visit(
        overloaded{
            [this, is_const](const Exp &node) {
                auto initializer = std::make_shared<sym::Initializer>();
                auto [exp_type, exp_value] =
                    is_const ? visit_const_exp(node) : visit_exp(node);
                if (exp_type->is_error()) {
                    return sym::InitializerPtr(nullptr);
                }
                initializer->insert(
                    sym::Initializer::InitValue(exp_type, exp_value));
                return initializer;
            },
            [this, &type, is_const](const ArrayInitVal &node) {
                if (!type.is_array()) {
                    error(-1, "cannot use array initializer on non-array type");
                    return sym::InitializerPtr(nullptr);
                }
                auto array_type = static_cast<sym::ArrayType &>(type);

                // get element number of array, for example, a[2][3] has 6
                // elements, so its initializer should have 6 elements
                auto initializer = std::make_shared<sym::Initializer>(
                    array_type.get_total_elm_count());
                for (auto &elm : node.items) {
                    // get initializer of each element
                    auto elm_initializer = visit_init_val(
                        *elm, *array_type.get_base_type(), is_const);
                    // then insert the initializer to the array initializer
                    if (elm_initializer) {
                        initializer->insert(*elm_initializer);
                    }
                }

                return initializer;
            },
        },
        node);
}

sym::TypePtr Visitor::visit_dims(const Dims &node, ASTType btype) {
    auto base_type = _asttype2symtype(btype);
    sym::TypeBuilder tb(base_type);

    // reverse is needed
    for (auto it = node.rbegin(); it != node.rend(); it++) {
        auto exp = *it;
        if (exp == nullptr) {
            tb.in_ptr();
        } else {
            auto [exp_type, exp_val] = visit_const_exp(*exp);
            if (exp_type->is_error()) {
                tb.in_array(1); // just to make it work
                continue;
            }

            if (!exp_type->is_int32() && !exp_type->is_float()) {
                error(-1, "array size must be int or float");
                tb.in_array(1); // just to make it work
                continue;
            }

            auto exp_const_val =
                std::dynamic_pointer_cast<ir::ConstBits>(exp_val);
            if (auto val = std::get_if<int>(&(exp_const_val->value))) {
                tb.in_array(*val);
            } else if (auto val = std::get_if<float>(&(exp_const_val->value))) {
                tb.in_array(*val); // implicit cast to int
            } else {
                throw std::logic_error("no value in const bits");
            }
        }
    }

    return tb.get_type();
}

void Visitor::visit_func_def(const FuncDef &node) {
    // get symbols of params
    auto param_symbols = visit_func_fparams(*node.func_fparams);

    // get function type
    auto return_type = _asttype2symtype(node.func_type);
    std::vector<sym::TypePtr> params_type;
    std::vector<ir::Type> params_ir_type;
    for (auto &param_symbol : param_symbols) {
        params_type.push_back(param_symbol->type);
        params_ir_type.push_back(_symtype2irtype(*param_symbol->type));
    }

    // add function symbol
    auto symbol = std::make_shared<sym::FunctionSymbol>(node.ident, params_type,
                                                        return_type);
    if (!_current_scope->add_symbol(symbol)) {
        error(-1, "redefine function " + node.ident);
        return;
    }
    _current_return_type = symbol->type;

    // create function in IR
    auto [ir_func, ir_params] = ir::Function::create(
        symbol->name == "main", symbol->name, _symtype2irtype(*return_type),
        params_ir_type, _module);
    symbol->value = ir_func->get_address();
    _builder.set_function(ir_func);

    // going into funciton scope
    _current_scope = _current_scope->push_scope();

    // add symbols of params to symbol table
    int no = 0;
    for (auto &param_symbol : param_symbols) {
        if (!_current_scope->add_symbol(param_symbol)) {
            error(-1, "redefine parameter " + param_symbol->name);
            no++;
            continue;
        }

        // alloc and store for param values
        auto param_ir_type = _symtype2irtype(*param_symbol->type);
        param_symbol->value = _builder.create_alloc(
            param_ir_type, param_symbol->type->get_size());
        _builder.create_store(param_ir_type, ir_params[no],
                              param_symbol->value);
        _last_store[param_symbol->value] = ir_params[no];

        no++;
    }

    _builder.create_label("body");

    visit_block_items(*node.block);

    if (ir_func->end->jump.type == ir::Jump::NONE) {
        ir_func->end->jump.type = ir::Jump::RET;
        if (node.ident == "main") {
            ir_func->end->jump.arg = ir::ConstBits::get(0);
        }
    }

    _current_scope = _current_scope->pop_scope();

    _builder.set_function(nullptr);

    _current_return_type = nullptr;
}

std::vector<sym::SymbolPtr>
Visitor::visit_func_fparams(const FuncFParams &node) {
    std::vector<sym::SymbolPtr> params;

    for (auto &elm : node) {
        auto type = visit_dims(*elm->dims, elm->btype);
        auto symbol = std::make_shared<sym::VariableSymbol>(elm->ident, type,
                                                            false, nullptr);
        params.push_back(symbol);
    }

    return params;
}

void Visitor::visit_block_items(const BlockItems &node) {
    for (auto &elm : node) {
        std::visit(overloaded{
                       [this](const Decl &node) { visit_decl(node); },
                       [this](const Stmt &node) { visit_stmt(node); },
                   },
                   *elm);
    }
}

void Visitor::visit_stmt(const Stmt &node) {
    std::visit(
        overloaded{
            [this](const AssignStmt &node) { visit_assign_stmt(node); },
            [this](const ExpStmt &node) { visit_exp_stmt(node); },
            [this](const BlockStmt &node) { visit_block_stmt(node); },
            [this](const IfStmt &node) { visit_if_stmt(node); },
            [this](const WhileStmt &node) { visit_while_stmt(node); },
            [this](const ControlStmt &node) { visit_control_stmt(node); },
            [this](const ReturnStmt &node) { visit_return_stmt(node); },
        },
        node);
}

ir::ValuePtr Visitor::_convert_if_needed(const sym::Type &to,
                                         const sym::Type &from,
                                         ir::ValuePtr val) {
    if (to == from) {
        return val;
    } else if (to.is_int32() && from.is_float()) {
        return _builder.create_stosi(val);
    } else if (to.is_float() && from.is_int32()) {
        return _builder.create_swtof(val);
    } else if (to.is_pointer() && (from.is_array() || from.is_pointer())) {
        return val; // pointer force cast
    } else {
        error(-1, "type convert not supported");
        return nullptr;
    }
}

void Visitor::visit_assign_stmt(const AssignStmt &node) {
    auto [exp_type, exp_val] = visit_exp(*node.exp);
    auto [lval_type, lval_val] = visit_lval(*node.lval);

    if (lval_type->is_error() || exp_type->is_error()) {
        return;
    }

    if (lval_type->is_array() || lval_type->is_pointer()) {
        error(-1, "left side of assignment is not a lval");
        return;
    }

    exp_val = _convert_if_needed(*lval_type, *exp_type, exp_val);
    if (!exp_val) {
        error(-1, "type not matched in assignment");
        return;
    }

    _builder.create_store(_symtype2irtype(*lval_type), exp_val, lval_val);
    _last_store[lval_val] = exp_val;
}

void Visitor::visit_exp_stmt(const ExpStmt &node) {
    if (node.exp) {
        visit_exp(*node.exp);
    }
}

void Visitor::visit_block_stmt(const BlockStmt &node) {
    _current_scope = _current_scope->push_scope();
    visit_block_items(*node.block);
    _current_scope = _current_scope->pop_scope();
}

void Visitor::visit_if_stmt(const IfStmt &node) {
    auto [jmp_to_true, jmp_to_false] = visit_cond(*node.cond);

    auto true_block = _builder.create_label("if_true");

    _current_scope = _current_scope->push_scope();
    visit_stmt(*node.if_stmt);
    _current_scope = _current_scope->pop_scope();

    ir::BlockPtr jmp_to_join;
    if (node.else_stmt) {
        jmp_to_join = _builder.create_jmp(nullptr);
    }

    auto false_block = _builder.create_label("if_false");

    if (node.else_stmt) {
        _current_scope = _current_scope->push_scope();
        visit_stmt(*node.else_stmt);
        _current_scope = _current_scope->pop_scope();

        auto join_block = _builder.create_label("if_join");
        // TODO: here is a bug, the type of jump could be RET
        jmp_to_join->jump.blk[0] = join_block;
    }

    // fill jump targets
    for (auto &jmp_block : jmp_to_true) {
        jmp_block->jump.blk[0] = true_block;
    }
    for (auto &jmp_block : jmp_to_false) {
        jmp_block->jump.blk[1] = false_block;
    }
}

void Visitor::visit_while_stmt(const WhileStmt &node) {
    if (_optimize) {
        int from, to;
        bool is_mini_loop;
        auto can_unroll_loop =
            !_in_unroll_loop && _can_unroll_loop(node, from, to, is_mini_loop);
        if (can_unroll_loop &&
            (to - from <= 10 || (is_mini_loop && to - from <= 110))) {
            _in_unroll_loop = true;
            for (int i = from; i < to; i++) {
                _current_scope = _current_scope->push_scope();
                visit_stmt(*node.stmt);
                _current_scope = _current_scope->pop_scope();
            }
            _in_unroll_loop = false;
            return;
        }

        // while with loop rotation
        auto jump_to_cond = _builder.create_jmp(nullptr);

        auto body_block = _builder.create_label("while_body");

        _while_stack.push(ContinueBreak({}, {}));

        if (can_unroll_loop) {
            _in_unroll_loop = true;
            constexpr auto get_max_factor = [](int num, int less_than = 10) {
                int max_factor = 1;
                for (int i = 2; i < num && i < less_than; i++) {
                    if (num % i == 0) {
                        max_factor = i;
                    }
                }
                return max_factor;
            };

            auto max_factor = get_max_factor(to - from);

            for (int i = 0; i < max_factor; i++) {
                _current_scope = _current_scope->push_scope();
                visit_stmt(*node.stmt);
                _current_scope = _current_scope->pop_scope();
            }
            _in_unroll_loop = false;
        } else {
            _current_scope = _current_scope->push_scope();
            visit_stmt(*node.stmt);
            _current_scope = _current_scope->pop_scope();
        }

        auto [continue_jmp, break_jmp] = _while_stack.top();
        _while_stack.pop();

        auto cond_block = _builder.create_label("while_cond");

        auto [jmp_to_true, jmp_to_false] = visit_cond(*node.cond);

        auto join_block = _builder.create_label("while_join");

        // fill jump targets
        jump_to_cond->jump.blk[0] = cond_block;
        for (auto &jmp_block : jmp_to_true) {
            jmp_block->jump.blk[0] = body_block;
        }
        for (auto &jmp_block : jmp_to_false) {
            jmp_block->jump.blk[1] = join_block;
        }
        for (auto &jmp_block : continue_jmp) {
            jmp_block->jump.blk[0] = cond_block;
        }
        for (auto &jmp_block : break_jmp) {
            jmp_block->jump.blk[0] = join_block;
        }
    } else {
        // normal while loop
        auto cond_block = _builder.create_label("while_cond");

        auto [jmp_to_true, jmp_to_false] = visit_cond(*node.cond);

        auto body_block = _builder.create_label("while_body");

        _while_stack.push(ContinueBreak({}, {}));

        _current_scope = _current_scope->push_scope();
        visit_stmt(*node.stmt);
        _current_scope = _current_scope->pop_scope();

        auto [continue_jmp, break_jmp] = _while_stack.top();
        _while_stack.pop();

        _builder.create_jmp(cond_block);

        auto join_block = _builder.create_label("while_join");

        // fill jump targets
        for (auto &jmp_block : jmp_to_true) {
            jmp_block->jump.blk[0] = body_block;
        }
        for (auto &jmp_block : jmp_to_false) {
            jmp_block->jump.blk[1] = join_block;
        }
        for (auto &jmp_block : continue_jmp) {
            jmp_block->jump.blk[0] = cond_block;
        }
        for (auto &jmp_block : break_jmp) {
            jmp_block->jump.blk[0] = join_block;
        }
    }
}

void Visitor::visit_control_stmt(const ControlStmt &node) {
    if (_while_stack.empty()) {
        error(-1, "break/continue statement not in while loop");
        return;
    }

    auto &[continue_jmp, break_jmp] = _while_stack.top();
    if (node.type == ControlStmt::BREAK) {
        break_jmp.push_back(_builder.create_jmp(nullptr));
    } else { // CONTINUE
        continue_jmp.push_back(_builder.create_jmp(nullptr));
    }
}

void Visitor::visit_return_stmt(const ReturnStmt &node) {
    if (node.exp) {
        auto [exp_type, exp_val] = visit_exp(*node.exp);
        if (exp_type->is_error()) {
            return;
        }

        exp_val = _convert_if_needed(*_current_return_type, *exp_type, exp_val);
        if (!exp_val) {
            error(-1, "type not matched in return statement");
            return;
        }

        _builder.create_ret(exp_val);
    }
    _builder.create_ret(nullptr);
}

ExpReturn Visitor::visit_const_exp(const Exp &node) {
    _require_const_lval++;
    auto [type, val] = visit_exp(node);
    _require_const_lval--;

    if (type->is_error()) {
        return ExpReturn(sym::ErrorType::get(), nullptr);
    }

    // check if the expression is constant
    if (auto const_val = std::dynamic_pointer_cast<ir::ConstBits>(val);
        !const_val) {
        error(-1, "not a const expression");
        return ExpReturn(sym::ErrorType::get(), nullptr);
    }

    return ExpReturn(type, val);
}

ExpReturn Visitor::visit_exp(const Exp &node) {
    return std::visit(
        overloaded{
            [this](const BinaryExp &node) { return visit_binary_exp(node); },
            [this](const LValExp &node) { return visit_lval_exp(node); },
            [this](const CallExp &node) { return visit_call_exp(node); },
            [this](const UnaryExp &node) { return visit_unary_exp(node); },
            [this](const CompareExp &node) { return visit_compare_exp(node); },
            [this](const Number &node) { return visit_number(node); },
        },
        node);
}

ExpReturn Visitor::visit_binary_exp(const BinaryExp &node) {
    auto [left_type, left_val] = visit_exp(*node.left);
    auto [right_type, right_val] = visit_exp(*node.right);

    if (left_type->is_error() || right_type->is_error()) {
        return ExpReturn(sym::ErrorType::get(), nullptr);
    }

    auto type = left_type->implicit_cast(*right_type);
    left_val = _convert_if_needed(*type, *left_type, left_val);
    if (!left_val) {
        error(-1, "type not matched in binary expression");
        return ExpReturn(sym::ErrorType::get(), nullptr);
    }
    right_val = _convert_if_needed(*type, *right_type, right_val);
    if (!right_val) {
        error(-1, "type not matched in binary expression");
        return ExpReturn(sym::ErrorType::get(), nullptr);
    }

    auto ir_type = _symtype2irtype(*type);
    switch (node.op) {
    case BinaryExp::ADD:
        return ExpReturn(type,
                         _builder.create_add(ir_type, left_val, right_val));
    case BinaryExp::SUB:
        return ExpReturn(type,
                         _builder.create_sub(ir_type, left_val, right_val));
    case BinaryExp::MULT:
        return ExpReturn(type,
                         _builder.create_mul(ir_type, left_val, right_val));
    case BinaryExp::DIV:
        return ExpReturn(type,
                         _builder.create_div(ir_type, left_val, right_val));
    case BinaryExp::MOD:
        if (!type->is_int32()) {
            error(-1, "mod operator % can only be used on int");
            return ExpReturn(sym::ErrorType::get(), nullptr);
        }
        return ExpReturn(type,
                         _builder.create_rem(ir_type, left_val, right_val));
    default:
        throw std::logic_error("unreachable");
    }
}

ExpReturn Visitor::visit_lval_exp(const LValExp &node) {
    if (_require_const_lval) {
        auto [type, values] = visit_const_lval(*node.lval);
        if (type->is_error()) {
            return ExpReturn(sym::ErrorType::get(), nullptr);
        }

        auto [_, val] = values[0];
        val = val ? val : ir::ConstBits::get(0);

        if (auto const_val = std::dynamic_pointer_cast<ir::ConstBits>(val);
            const_val) {
            auto elm_ir_type = _symtype2irtype(*type);
            return ExpReturn(type, _convert_const(elm_ir_type, *const_val));
        } else {
            error(-1, "not a const expression");
            return ExpReturn(sym::ErrorType::get(), nullptr);
        }
    } else {
        auto [type, val] = visit_lval(*node.lval);
        if (type->is_error()) {
            return ExpReturn(sym::ErrorType::get(), nullptr);
        }

        // still an array, the value is the address itself
        if (type->is_array()) {
            return ExpReturn(type, val);
        }

        // since lval is in an expression, we need to get the value from the
        // lval address
        auto exp_val = _builder.create_load(_symtype2irtype(*type), val);

        return ExpReturn(type, exp_val);
    }
}

ExpReturn Visitor::visit_lval(const LVal &node) {
    return std::visit(
        overloaded{
            [this](const Ident &node) {
                auto symbol = _current_scope->get_symbol(node);
                if (!symbol) {
                    error(-1, "undefined symbol " + node);
                    return ExpReturn(sym::ErrorType::get(), nullptr);
                }

                return ExpReturn(symbol->type, symbol->value);
            },
            [this](const Index &node) {
                auto [lval_type, lval_val] = visit_lval(*node.lval);
                auto [exp_type, exp_val] = visit_exp(*node.exp);

                if (lval_type->is_error() || exp_type->is_error()) {
                    return ExpReturn(sym::ErrorType::get(), nullptr);
                }

                if (!lval_type->is_array() && !lval_type->is_pointer()) {
                    error(-1, "index operator [] can only be used on "
                              "array or pointer");
                    return ExpReturn(sym::ErrorType::get(), nullptr);
                }

                // calc the address through index. if the curr lval
                // is a pointer, don't forget to get the value from
                // pointer first
                if (lval_type->is_pointer()) {
                    lval_val = _builder.create_load(ir::Type::L, lval_val);
                }

                auto lval_indirect_type =
                    std::static_pointer_cast<sym::IndirectType>(lval_type);

                // since exp_type is int, we need to convert it to long
                exp_val = _builder.create_extsw(exp_val);

                auto elm_size = ir::ConstBits::get(
                    lval_indirect_type->get_base_type()->get_size());

                auto offset =
                    _builder.create_mul(ir::Type::L, exp_val, elm_size);
                auto val = _builder.create_add(ir::Type::L, lval_val, offset);

                // addr = base_addr + index * elm_size
                return ExpReturn(lval_indirect_type->get_base_type(), val);
            }},
        node);
}

ConstLValReturn Visitor::visit_const_lval(const LVal &node) {
    return std::visit(
        overloaded{
            [this](const Ident &node) {
                auto symbol = _current_scope->get_symbol(node);
                if (!symbol) {
                    error(-1, "undefined symbol " + node);
                    return ConstLValReturn(sym::ErrorType::get(), {});
                }

                auto var_symbol =
                    std::static_pointer_cast<sym::VariableSymbol>(symbol);

                if (!var_symbol->is_constant) {
                    return ConstLValReturn(sym::ErrorType::get(), {});
                }

                return ConstLValReturn(var_symbol->type,
                                       var_symbol->initializer->get_values());
            },
            [this](const Index &node) {
                auto [lval_type, lval_val] = visit_const_lval(*node.lval);
                auto [exp_type, exp_val] = visit_const_exp(*node.exp);

                if (lval_type->is_error() || exp_type->is_error()) {
                    return ConstLValReturn(sym::ErrorType::get(), {});
                }

                if (!lval_type->is_array()) {
                    error(-1, "index operator [] can only be used on "
                              "array or pointer");
                    return ConstLValReturn(sym::ErrorType::get(), {});
                }

                auto lval_array_type =
                    std::static_pointer_cast<sym::ArrayType>(lval_type);

                auto elm_size =
                    lval_array_type->get_base_type()->get_size() / 4;

                auto index = std::get<int>(
                    std::static_pointer_cast<ir::ConstBits>(exp_val)->value);
                index *= elm_size;

                // get all values in the range [index, index + elm_size)
                auto range_from = lval_val.lower_bound(index);
                auto range_to =
                    lval_val.lower_bound(index + elm_size); // this is not wrong

                std::map<int, sym::Initializer::InitValue> values;
                for (auto it = range_from; it != range_to; it++) {
                    auto [no, val] = *it;
                    values.insert({no - index, val});
                }

                return ConstLValReturn(lval_array_type->get_base_type(),
                                       values);
            },
        },
        node);
}

ExpReturn Visitor::visit_call_exp(const CallExp &node) {
    auto symbol = _current_scope->get_symbol(node.ident);
    if (!symbol) {
        error(-1, "undefined function " + node.ident);
        return ExpReturn(sym::ErrorType::get(), nullptr);
    }

    auto func_symbol = std::dynamic_pointer_cast<sym::FunctionSymbol>(symbol);
    if (!func_symbol) {
        error(-1, node.ident + " is not a function");
        return ExpReturn(sym::ErrorType::get(), nullptr);
    }

    auto params_type = func_symbol->param_types;

    if (params_type.size() != node.func_rparams->size()) {
        error(-1, "params number not matched in function call " + node.ident);
        return ExpReturn(sym::ErrorType::get(), nullptr);
    }

    std::vector<ir::ValuePtr> ir_args;
    for (int i = 0; i < params_type.size(); i++) {
        auto [exp_type, exp_val] = visit_exp(*node.func_rparams->at(i));

        if (exp_type->is_error()) {
            return ExpReturn(sym::ErrorType::get(), nullptr);
        }

        if (*exp_type != *(params_type[i])) {
            exp_val = _convert_if_needed(*params_type[i], *exp_type, exp_val);

            if (!exp_val) {
                error(-1, "params type not matched in function call " +
                              node.ident + ", expected " +
                              params_type[i]->tostring() + ", got " +
                              exp_type->tostring());
                return ExpReturn(sym::ErrorType::get(), nullptr);
            }
        }

        ir_args.push_back(exp_val);
    }

    if (node.ident == "starttime" || node.ident == "stoptime") { // very ugly
        return ExpReturn(
            symbol->type,
            _builder.create_call(_symtype2irtype(*func_symbol->type),
                                 func_symbol->value, {ir::ConstBits::get(0)}));
    }

    auto ir_ret = _builder.create_call(_symtype2irtype(*func_symbol->type),
                                       func_symbol->value, ir_args);

    return ExpReturn(symbol->type, ir_ret);
}

ExpReturn Visitor::visit_unary_exp(const UnaryExp &node) {
    auto [exp_type, exp_val] = visit_exp(*node.exp);

    if (exp_type->is_error()) {
        return ExpReturn(sym::ErrorType::get(), nullptr);
    }

    switch (node.op) {
    case UnaryExp::ADD:
        return ExpReturn(exp_type, exp_val); // do nothing
    case UnaryExp::SUB:
        if (!exp_type->is_int32() && !exp_type->is_float()) {
            error(-1, "neg operator - can only be used on int or float");
            return ExpReturn(sym::ErrorType::get(), nullptr);
        }
        return ExpReturn(
            exp_type, _builder.create_neg(_symtype2irtype(*exp_type), exp_val));
    case UnaryExp::NOT:
        if (exp_type->is_int32()) {
            // !a equal to (a == 0)
            return ExpReturn(
                exp_type, _builder.create_ceqw(exp_val, ir::ConstBits::get(0)));
        } else if (exp_type->is_float()) {
            // !a equal to (a == 0)
            auto cmp_val =
                _builder.create_ceqs(exp_val, ir::ConstBits::get(0.0f));
            // int to float
            return ExpReturn(exp_type, _builder.create_swtof(cmp_val));
        } else {
            error(-1, "not operator ! can only be used on int or float");
            return ExpReturn(sym::ErrorType::get(), nullptr);
        }
    default:
        throw std::logic_error("unreachable");
    }
}

ExpReturn Visitor::visit_compare_exp(const CompareExp &node) {
    auto [left_type, left_val] = visit_exp(*node.left);
    auto [right_type, right_val] = visit_exp(*node.right);

    if (left_type->is_error() || right_type->is_error()) {
        return ExpReturn(sym::ErrorType::get(), nullptr);
    }

    auto type = left_type->implicit_cast(*right_type);
    left_val = _convert_if_needed(*type, *left_type, left_val);
    if (!left_val) {
        error(-1, "type not matched in compare expression");
        return ExpReturn(sym::ErrorType::get(), nullptr);
    }
    right_val = _convert_if_needed(*type, *right_type, right_val);
    if (!right_val) {
        error(-1, "type not matched in compare expression");
        return ExpReturn(sym::ErrorType::get(), nullptr);
    }

    if (!type->is_int32() && !type->is_float()) {
        error(-1, "compare operator can only be used on int or float");
        return ExpReturn(sym::ErrorType::get(), nullptr);
    }

    if (type->is_int32()) {
        switch (node.op) {
        case CompareExp::EQ:
            return ExpReturn(sym::Int32Type::get(),
                             _builder.create_ceqw(left_val, right_val));
        case CompareExp::NE:
            return ExpReturn(sym::Int32Type::get(),
                             _builder.create_cnew(left_val, right_val));
        case CompareExp::LT:
            return ExpReturn(sym::Int32Type::get(),
                             _builder.create_csltw(left_val, right_val));
        case CompareExp::LE:
            return ExpReturn(sym::Int32Type::get(),
                             _builder.create_cslew(left_val, right_val));
        case CompareExp::GT:
            return ExpReturn(sym::Int32Type::get(),
                             _builder.create_csgtw(left_val, right_val));
        case CompareExp::GE:
            return ExpReturn(sym::Int32Type::get(),
                             _builder.create_csgew(left_val, right_val));
        default:
            throw std::logic_error("unreachable");
        }
    } else { // float
        switch (node.op) {
        case CompareExp::EQ:
            return ExpReturn(sym::Int32Type::get(),
                             _builder.create_ceqs(left_val, right_val));
        case CompareExp::NE:
            return ExpReturn(sym::Int32Type::get(),
                             _builder.create_cnes(left_val, right_val));
        case CompareExp::LT:
            return ExpReturn(sym::Int32Type::get(),
                             _builder.create_clts(left_val, right_val));
        case CompareExp::LE:
            return ExpReturn(sym::Int32Type::get(),
                             _builder.create_cles(left_val, right_val));
        case CompareExp::GT:
            return ExpReturn(sym::Int32Type::get(),
                             _builder.create_cgts(left_val, right_val));
        case CompareExp::GE:
            return ExpReturn(sym::Int32Type::get(),
                             _builder.create_cges(left_val, right_val));
        default:
            throw std::logic_error("unreachable");
        }
    }
}

ExpReturn Visitor::visit_number(const Number &node) {
    return std::visit(overloaded{
                          [this](int node) {
                              return ExpReturn(sym::Int32Type::get(),
                                               ir::ConstBits::get(node));
                          },
                          [this](float node) {
                              return ExpReturn(sym::FloatType::get(),
                                               ir::ConstBits::get(node));
                          },
                      },
                      node);
}

CondReturn Visitor::visit_cond(const Cond &node) {
    return std::visit(
        overloaded{
            [this](const Exp &node) {
                auto [type, val] = visit_exp(node);
                if (type->is_error()) {
                    return CondReturn(BlockPtrList{}, BlockPtrList{});
                }

                if (type->is_float()) {
                    val = _builder.create_cnes(val, ir::ConstBits::get(0.0f));
                } else if (!type->is_int32()) {
                    error(-1, "condition must be int or float");
                    return CondReturn(BlockPtrList{}, BlockPtrList{});
                }

                // generate conditional jump instruction
                auto jnz_block = _builder.create_jnz(val, nullptr, nullptr);
                return CondReturn(BlockPtrList{jnz_block},
                                  BlockPtrList{jnz_block});
            },
            [this](const LogicalExp &node) { return visit_logical_exp(node); },
        },
        node);
}

CondReturn Visitor::visit_logical_exp(const LogicalExp &node) {
    auto [ltruelist, lfalselist] = visit_cond(*node.left);

    // create a new block for the right expression
    auto logic_right_block = _builder.create_label("logic_right");

    auto [rtruelist, rfalselist] = visit_cond(*node.right);

    BlockPtrList truelist, falselist;
    if (node.op == LogicalExp::AND) {
        // if the logical expression is `&&`, we need to merge the false
        // list from left and right expression
        falselist.assign(lfalselist.begin(), lfalselist.end());
        falselist.insert(falselist.end(), rfalselist.begin(), rfalselist.end());

        // as for the true list from left expression, just jump to the right
        // expression
        for (auto &jmp_block : ltruelist) {
            jmp_block->jump.blk[0] = logic_right_block;
        }

        // and the true list from right expression is the true list of the
        // whole expression
        truelist.assign(rtruelist.begin(), rtruelist.end());
    } else { // OR
        // vice versa, if the logical expression is `||`, we need to merge
        // the true list from left and right expression
        truelist.assign(ltruelist.begin(), ltruelist.end());
        truelist.insert(truelist.end(), rtruelist.begin(), rtruelist.end());

        // as for the false list from left expression, just jump to the
        // right expression
        for (auto &jmp_block : lfalselist) {
            jmp_block->jump.blk[1] = logic_right_block;
        }

        // and the false list from right expression is the false list of the
        // whole expression
        falselist.assign(rfalselist.begin(), rfalselist.end());
    }

    // since if and while stmt will always create a new block for the body,
    // we don't need to create a new block after the right expression.
    // just comment the following line

    // _builder.create_label("logic_join");

    return CondReturn(truelist, falselist);
}

void Visitor::_add_builtin_funcs() {
    // add built-in functions declaration to the global scope
    using Params = std::vector<sym::TypePtr>;
    using FuncSym = sym::FunctionSymbol;

    auto intty = sym::Int32Type::get();
    auto floatty = sym::FloatType::get();
    auto voidty = sym::VoidType::get();
    auto intp = sym::TypeBuilder(intty).in_ptr().get_type();
    auto floatp = sym::TypeBuilder(floatty).in_ptr().get_type();

    auto getint = std::make_shared<FuncSym>("getint", Params{}, intty);
    auto getch = std::make_shared<FuncSym>("getch", Params{}, intty);
    auto getfloat = std::make_shared<FuncSym>("getfloat", Params{}, floatty);
    auto getarray = std::make_shared<FuncSym>("getarray", Params{intp}, intty);
    auto getfarray =
        std::make_shared<FuncSym>("getfarray", Params{floatp}, intty);
    auto putint = std::make_shared<FuncSym>("putint", Params{intty}, voidty);
    auto putch = std::make_shared<FuncSym>("putch", Params{intty}, voidty);
    auto putfloat =
        std::make_shared<FuncSym>("putfloat", Params{floatty}, voidty);
    auto putarray =
        std::make_shared<FuncSym>("putarray", Params{intty, intp}, voidty);
    auto putfarray =
        std::make_shared<FuncSym>("putfarray", Params{intty, floatp}, voidty);
    auto starttime = std::make_shared<FuncSym>("starttime", Params{}, voidty);
    starttime->value = ir::Address::get("_sysy_starttime");
    auto stoptime = std::make_shared<FuncSym>("stoptime", Params{}, voidty);
    stoptime->value = ir::Address::get("_sysy_stoptime");

    auto builtin_funcs = std::vector<sym::SymbolPtr>{
        getint, getch,    getfloat, getarray,  getfarray, putint,
        putch,  putfloat, putarray, putfarray, starttime, stoptime,
    };

    for (auto &func : builtin_funcs) {
        if (func->value == nullptr) {
            func->value = ir::Address::get(func->name);
        }
        _current_scope->add_symbol(func);
    }
}
