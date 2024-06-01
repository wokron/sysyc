#include "midend/symbol.h"
#include "midend/type.h"
#include <doctest.h>

TEST_CASE("testing symbol table") {
    auto int_type = Int32Type::get();

    auto table = std::make_shared<SymbolTable>();
    CHECK_FALSE(table->has_parent());

    auto sym = std::make_shared<VariableSymbol>("sym", int_type, false, nullptr);
    table->add_symbol(sym);
    CHECK(table->exist_in_scope("sym"));

    table = table->push_scope();
    CHECK(table->has_parent());
    CHECK_FALSE(table->exist_in_scope("sym"));

    auto sym1 = std::make_shared<VariableSymbol>("sym1", int_type, false, nullptr);
    CHECK(sym1->tostring() == "var sym1 int");

    table->add_symbol(sym1);
    CHECK(table->exist_in_scope("sym1"));

    auto tmp = table->get_symbol("sym1");
    CHECK(tmp == sym1);

    auto sym2 = std::make_shared<VariableSymbol>("sym2", int_type, false, nullptr);
    table->add_symbol(sym2);
    CHECK(table->exist_in_scope("sym2"));

    table = table->pop_scope();
    CHECK(table->exist_in_scope("sym"));
    CHECK_FALSE(table->exist_in_scope("sym1"));
    CHECK_FALSE(table->exist_in_scope("sym2"));

    auto params = std::vector<std::shared_ptr<Type>>{int_type, int_type};
    auto sym3 = std::make_shared<FunctionSymbol>("sym3", params, int_type);

    CHECK(sym3->tostring() == "func sym3(int, int) int");
}

TEST_CASE("testing initializer") {
    // int a[3][3] = {1, 1, 1, {1, 1}, {1}};
    Initializer init(3 * 3);
    Initializer init1(3);
    Initializer init2(3);

    CHECK(init.insert(Initializer::value_t(nullptr, nullptr)));
    CHECK(init.insert(Initializer::value_t(nullptr, nullptr)));
    CHECK(init.insert(Initializer::value_t(nullptr, nullptr)));

    CHECK(init1.insert(Initializer::value_t(nullptr, nullptr)));
    CHECK(init1.insert(Initializer::value_t(nullptr, nullptr)));
    CHECK(init.insert(init1));

    CHECK(init2.insert(Initializer::value_t(nullptr, nullptr)));
    CHECK(init.insert(init2));

    int a[9] = {1, 1, 1, 1, 1, 0, 1, 0, 0};
    for (auto &kv : init.get_values()) {
        CHECK(a[kv.first] == 1);
    }
}