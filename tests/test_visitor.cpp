#include "ast_visitor.h"
#include "parser.h"
#include <doctest.h>

extern FILE *yyin;

TEST_CASE("testing ast visitor") {
    const char *content = R"(
        int a = 10;
        int main() {
            int c[3][3] = {1, 2, 3, {3}, {4, 5}};
            return c[0][0];
        }
    )";

    FILE *testfile = fopen("test_frontend.sy", "w");
    fprintf(testfile, "%s", content);
    fclose(testfile);

    yyin = fopen("test_frontend.sy", "r");

    auto root = std::make_shared<CompUnits>();
    yyparse(root);

    fclose(yyin);
    yyin = nullptr;
    remove("test_frontend.sy");

    ir::Module module;

    ASTVisitor visitor(module);

    visitor.visit(*root);
}