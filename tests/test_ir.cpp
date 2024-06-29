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
    jmp @body.2
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

    auto [func, _] =
        ir::Function::create(false, "func", ir::Type::X, {}, module);

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

static constexpr char EXPECTED4[] = R"(function $func(w %.1, s %.2, l %.3, ) {
@start.1
    %.4 =l alloc4 4
    storew %.1, %.4
    %.5 =l alloc4 4
    stores %.2, %.5
    %.6 =l alloc8 8
    storel %.3, %.6
    jmp @body.2
@body.2
    %.7 =w loadw %.4
    %.8 =w add %.7, %.7
    ret %.8
}
)";

TEST_CASE("testing load and store") {
    auto module = ir::Module();

    auto [func, params] =
        ir::Function::create(false, "func", ir::Type::X,
                             {ir::Type::W, ir::Type::S, ir::Type::L}, module);

    auto builder = ir::IRBuilder(func);

    auto par1 = builder.create_alloc(ir::Type::W, 1 * 4);
    builder.create_store(ir::Type::W, params[0], par1);

    auto par2 = builder.create_alloc(ir::Type::S, 1 * 4);
    builder.create_store(ir::Type::S, params[1], par2);

    auto par3 = builder.create_alloc(ir::Type::L, 1 * 8);
    builder.create_store(ir::Type::L, params[2], par3);

    auto body = builder.create_label("body");

    auto loadpar1 = builder.create_load(ir::Type::W, par1);

    auto add = builder.create_add(ir::Type::W, loadpar1, loadpar1);

    auto ret = builder.create_ret(add);

    std::ostringstream out;
    module.emit(out);
    CHECK_EQ(out.str(), EXPECTED4);
}

static constexpr char EXPECTED5[] = R"(function w $func(w %.1, w %.2, ) {
@start.1
    jmp @body.2
@body.2
    %.3 =w call $func(w %.2, w %.1, )
    ret %.3
}
)";

TEST_CASE("testing function call") {
    auto module = ir::Module();

    auto [func, params] = ir::Function::create(
        false, "func", ir::Type::W, {ir::Type::W, ir::Type::W}, module);

    auto builder = ir::IRBuilder(func);

    auto body = builder.create_label("body");

    auto call = builder.create_call(ir::Type::W, func->get_address(),
                                    {params[1], params[0]});

    auto ret = builder.create_ret(call);

    std::ostringstream out;
    module.emit(out);
    CHECK_EQ(out.str(), EXPECTED5);
}