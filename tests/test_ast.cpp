#include "ast.h"
#include <cstring>
#include <doctest.h>
#include <sstream>

template <typename T> using sp = std::shared_ptr<T>;

TEST_CASE("testing ast build and print") {
    auto root = sp<CompUnits>(new CompUnits{
        sp<CompUnit>(new CompUnit(
            Decl{Decl::CONST, ASTType::FLOAT,
                 sp<VarDefs>(new VarDefs{sp<VarDef>(new VarDef{
                     Ident("x"), sp<Dims>(new Dims{}), sp<InitVal>()})})})),
        sp<CompUnit>(new CompUnit(FuncDef{ASTType::INT, Ident("func1"),
                                          sp<FuncFParams>(new FuncFParams{}),
                                          sp<BlockItems>(new BlockItems{})}))});

    std::ostringstream ss;
    print_ast(ss, *root);

    auto ast_str = ss.str();

    CHECK(ast_str ==
          "{\"comp_units\":[{\"type\":0,\"btype\":1,\"var_defs\":[{\"ident\":"
          "\"x\",\"dims\":[],\"init_val\":null}]},{\"btype\":0,\"ident\":"
          "\"func1\",\"func_fparams\":[],\"block\":[]}]}");
}
