#include <parser.h>
#include <ast.h>
#include <iostream>

int main() {
    auto root = std::make_shared<CompUnits>();
    yyparse(root);

    print_ast(std::cout, *root);
    return 0;
}
