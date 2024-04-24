#include "ast.h"
#include <cstring>
#include <doctest.h>
#include <sstream>

TEST_CASE("testing ast alloc, print and free") {
    CompUnit *root = new AST(CompUnit, DECL, {
        new AST(CompUnit, FUNC, {
            nullptr,
            new FuncDef{
                ASTType::VOID,
                strdup("a"),
                nullptr,
                new BlockItems{
                    nullptr,
                    nullptr,
                }
            }
        }),
        new Decl{
            Decl::CONST,
            ASTType::INT,
            new VarDefs{
                nullptr,
                new VarDef{
                    strdup("a"),
                    nullptr,
                    nullptr,
                }
        }}
    });

    std::ostringstream ss;
    root->print(ss);

    auto result = ss.str();

    CHECK(
        result ==
        "{\"comp_unit\": {\"comp_unit\": nullptr,\"func_def\": {\"func_type\": "
        "2,\"ident\": a,\"func_fparams\": nullptr,\"block\": {\"block_items\": "
        "nullptr,\"block_item\": nullptr}}},\"decl\": {\"var_type\": "
        "0,\"btype\": 0,\"var_defs\": {\"var_defs\": nullptr,\"var_def\": "
        "{\"ident\": a,\"dims\": nullptr,\"init_val\": nullptr}}}}");
    
    delete root;
}
