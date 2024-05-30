#include "ir/builder.h"
#include "ir/ir.h"
#include <doctest.h>
#include <sstream>

static constexpr char EXPECTED[] = R"(function w $add(w %.1, w %.2, ) {
@start.1
    %.3 =w add %.1, %.2
    ret %.3
}
export
function w $main() {
@start.2
    %.1 =w call $add(w 1, w 1, )
    ret %.1
}
)";

TEST_CASE("testing type to string") {
    CHECK_EQ(ir::type_to_string(ir::Type::X), "x");
    CHECK_EQ(ir::type_to_string(ir::Type::W), "w");
    CHECK_EQ(ir::type_to_string(ir::Type::L), "l");
    CHECK_EQ(ir::type_to_string(ir::Type::S), "s");
}

TEST_CASE("testing ir building") {
    auto module = ir::Module();

    auto [add, params] = ir::Function::create(
        false, "add", ir::Type::W, {ir::Type::W, ir::Type::W}, module);
    auto builder = ir::IRBuilder(add);

    auto start = add->start;
    builder.set_insert_point(start);

    auto c = builder.create_add(ir::Type::W, params[0], params[1]);
    builder.create_ret(c);

    auto [main, _] =
        ir::Function::create(true, "main", ir::Type::W, {}, module);
    builder = ir::IRBuilder(main);
    start = main->start;
    builder.set_insert_point(start);
    auto r =
        builder.create_call(ir::Type::W, ir::Address::get(add->name),
                            {ir::ConstBits::get(1), ir::ConstBits::get(1)});
    builder.create_ret(r);

    std::ostringstream out;
    module.emit(out);
    CHECK_EQ(out.str(), EXPECTED);
}

static constexpr char EXPECTED2[] = R"(function $func() {
@start.1
}
)";

static constexpr char EXPECTED3[] = R"(function $func() {
@start.1
@body.2
    jnz 1, @if_true.3, @if_false.4
@if_true.3
    ret 1
@if_false.4
    ret 0
}
)";

TEST_CASE("testing basic blocks") {
    auto module = ir::Module();

    auto [func, params] = ir::Function::create(
        false, "func", ir::Type::X, {}, module);

    std::ostringstream out1;
    module.emit(out1);
    CHECK_EQ(out1.str(), EXPECTED2);

    auto builder = ir::IRBuilder(func);

    auto body = builder.create_label("body");

    auto jnz = builder.create_jnz(ir::ConstBits::get(1), nullptr, nullptr);

    auto if_true = builder.create_label("if_true");

    auto true_ret = builder.create_ret(ir::ConstBits::get(1));

    auto if_false = builder.create_label("if_false");

    auto false_ret = builder.create_ret(ir::ConstBits::get(0));

    jnz->jump.blk[0] = if_true;
    jnz->jump.blk[1] = if_false;

    std::ostringstream out2;
    module.emit(out2);
    CHECK_EQ(out2.str(), EXPECTED3);
}