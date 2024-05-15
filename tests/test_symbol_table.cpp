#include "midend/symbol.h"
#include "midend/type.h"
#include <doctest.h>

TEST_CASE("testing symbol table") {
    auto int_type = std::make_shared<Int32Type>();

    auto table = std::make_shared<SymbolTable>();
    CHECK_FALSE(table->has_parent());

    auto sym = std::make_shared<VariableSymbol>("sym", int_type, false);
    table->add_symbol(sym);
    CHECK(table->exist_in_scope("sym"));

    table = table->push_scope();
    CHECK(table->has_parent());
    CHECK_FALSE(table->exist_in_scope("sym"));

    auto sym1 = std::make_shared<VariableSymbol>("sym1", int_type, false);
    CHECK(sym1->tostring() == "var sym1 int");

    table->add_symbol(sym1);
    CHECK(table->exist_in_scope("sym1"));

    auto tmp = table->get_symbol("sym1");
    CHECK(tmp == sym1);

    auto sym2 = std::make_shared<VariableSymbol>("sym2", int_type, false);
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