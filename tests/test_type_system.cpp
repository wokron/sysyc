#include "midend/type.h"
#include <doctest.h>

TEST_CASE("testing type system") {
    auto int_type = std::make_shared<Int32Type>();
    auto float_type = std::make_shared<FloatType>();
    auto void_type = std::make_shared<VoidType>();

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

    auto ref = std::dynamic_pointer_cast<ReferenceType>(t1)->get_ref_type();
    CHECK(ref->tostring() == "[10][5][3]int");
    CHECK(ref->get_size() == 10 * 5 * 3 * 4);

    auto t2 = TypeBuilder(float_type).in_array(3).in_array(5).get_type();
    CHECK(t2->tostring() == "[5][3]float");
    CHECK(t2->get_size() == 5 * 3 * 4);
}