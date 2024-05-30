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