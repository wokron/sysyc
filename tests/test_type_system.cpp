#include "sym/type.h"
#include <doctest.h>

using namespace sym;

TEST_CASE("testing type printing and size calculation") {
    auto int_type = Int32Type::get();
    auto float_type = FloatType::get();
    auto void_type = VoidType::get();

    CHECK(int_type->tostring() == "int");
    CHECK(float_type->tostring() == "float");
    CHECK(void_type->tostring() == "void");

    auto t1 = TypeBuilder(int_type)
                  .in_array(3)
                  .in_array(5)
                  .in_array(10)
                  .in_ptr()
                  .get_type();
    CHECK(t1->tostring() == "*[10][5][3]int");
    CHECK(t1->get_size() == 8);

    auto ref = std::dynamic_pointer_cast<IndirectType>(t1)->get_base_type();
    CHECK(ref->tostring() == "[10][5][3]int");
    CHECK(ref->get_size() == 10 * 5 * 3 * 4);

    auto t2 = TypeBuilder(float_type).in_array(3).in_array(5).get_type();
    CHECK(t2->tostring() == "[5][3]float");
    CHECK(t2->get_size() == 5 * 3 * 4);
}

TEST_CASE("testing type equality") {
    auto int_type = Int32Type::get();
    auto float_type = FloatType::get();
    auto void_type = VoidType::get();

    CHECK(*int_type == *Int32Type::get());
    CHECK(*float_type == *FloatType::get());
    CHECK(*void_type == *VoidType::get());

    auto t1 = TypeBuilder(int_type)
                  .in_array(3)
                  .in_array(5)
                  .in_array(10)
                  .in_ptr()
                  .get_type();
    auto t2 = TypeBuilder(int_type)
                  .in_array(3)
                  .in_array(5)
                  .in_array(10)
                  .in_ptr()
                  .get_type();
    CHECK(*t1 == *t2);

    auto t3 = TypeBuilder(float_type).in_array(3).in_array(5).get_type();
    auto t4 = TypeBuilder(float_type).in_array(3).in_array(5).get_type();
    CHECK(*t3 == *t4);

    CHECK(*t1 != *t3);
    CHECK(*t2 != *t4);
}