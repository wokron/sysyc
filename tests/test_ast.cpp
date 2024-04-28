#include "ast.h"
#include <cstring>
#include <doctest.h>
#include <sstream>

TEST_CASE("testing ast alloc, print and free") {
    // CompUnits *root = new CompUnits{{
    //     new AST(CompUnit, DECL,
    //             new Decl{Decl::CONST, ASTType::INT,
    //                      new VarDefs{{new VarDef{
    //                          strdup("a"),
    //                          nullptr,
    //                          nullptr,
    //                      }}}}),
    //     new AST(CompUnit, FUNC,
    //             new FuncDef{ASTType::VOID, strdup("a"), new FuncFParams{{}},
    //                         new BlockItems{{}}}),
    // }};

    // std::ostringstream ss;
    // root->print(ss);

    // auto result = ss.str();

    // CHECK(
    //     result ==
    //     "[{\"decl\": {\"var_type\": 0,\"btype\": 0,\"var_defs\": [{\"ident\": "
    //     "a,\"dims\": nullptr,\"init_val\": nullptr}]}},{\"func_def\": "
    //     "{\"func_type\": 2,\"ident\": a,\"func_fparams\": [],\"block\": []}}]");

    // delete root;
}
