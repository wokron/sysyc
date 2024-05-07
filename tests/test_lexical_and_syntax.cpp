#include "parser.h"
#include <cstdio>
#include <cstdlib>
#include <doctest.h>
#include <sstream>

extern FILE *yyin;

const char *content = "int a = 10;\n"
                      "const int b = 20;\n"
                      "const int c[10] = {1, 2, 3};\n"
                      "int d[10][4 / 2] = {1, 2, 3, {4, 5}, 6};\n"
                      "int main(int a, float b, float c[], int d[][2]) {\n"
                      "    while (1) {\n"
                      "        return 0;\n"
                      "    }\n"
                      "    if (a < b && c > d || e == f) {\n"
                      "        return a + b * c - 10;\n"
                      "    } else {\n"
                      "        return 3.14e-10 + 0x123.4P10;\n"
                      "    }\n"
                      "}\n"
                      "void func() {\n"
                      "    return 0;\n"
                      "}";

TEST_CASE("testing lexical and syntax analysis") {
    FILE *testfile = fopen("test_frontend.sy", "w");
    fprintf(testfile, "%s", content);
    fclose(testfile);

    yyin = fopen("test_frontend.sy", "r");

    auto root = std::make_shared<CompUnits>();
    yyparse(root);

    fclose(yyin);
    yyin = NULL;

    std::ostringstream ss;
    print_ast(ss, *root);

    auto ast_str = ss.str();

    CHECK(
        ast_str ==
        "{\"comp_units\":[{\"type\":1,\"btype\":0,\"var_defs\":[{\"ident\":"
        "\"a\",\"dims\":[],\"init_val\":10}]},{\"type\":0,\"btype\":0,\"var_"
        "defs\":[{\"ident\":\"b\",\"dims\":[],\"init_val\":20}]},{\"type\":0,"
        "\"btype\":0,\"var_defs\":[{\"ident\":\"c\",\"dims\":[10],\"init_val\":"
        "{\"items\":[1,2,3]}}]},{\"type\":1,\"btype\":0,\"var_defs\":[{"
        "\"ident\":\"d\",\"dims\":[10,{\"left\":4,\"right\":2,\"binary_op\":3}]"
        ",\"init_val\":{\"items\":[1,2,3,{\"items\":[4,5]},6]}}]},{\"btype\":0,"
        "\"ident\":\"main\",\"func_fparams\":[{\"btype\":0,\"ident\":\"a\","
        "\"dims\":[]},{\"btype\":1,\"ident\":\"b\",\"dims\":[]},{\"btype\":1,"
        "\"ident\":\"c\",\"dims\":[null]},{\"btype\":0,\"ident\":\"d\","
        "\"dims\":[null,2]}],\"block\":[{\"cond\":1,\"stmt\":{\"block\":[{"
        "\"exp\":0}]}},{\"cond\":{\"left\":{\"left\":{\"left\":\"a\",\"right\":"
        "\"b\",\"compare_op\":2},\"right\":{\"left\":\"c\",\"right\":\"d\","
        "\"compare_op\":4},\"bool_op\":0},\"right\":{\"left\":\"e\",\"right\":"
        "\"f\",\"compare_op\":0},\"bool_op\":1},\"if_stmt\":{\"block\":[{"
        "\"exp\":{\"left\":{\"left\":\"a\",\"right\":{\"left\":\"b\",\"right\":"
        "\"c\",\"binary_op\":2},\"binary_op\":0},\"right\":10,\"binary_op\":1}}"
        "]},\"else_stmt\":{\"block\":[{\"exp\":{\"left\":3.14e-10,\"right\":"
        "298240,\"binary_op\":0}}]}}]},{\"btype\":2,\"ident\":\"func\",\"func_"
        "fparams\":[],\"block\":[{\"exp\":0}]}]}");
}
