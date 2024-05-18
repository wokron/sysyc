%define parse.trace

/* return from param */
%parse-param {std::shared_ptr<CompUnits> comp_units}

%code requires {
    #include "ast.h"
    #include <memory>
    void yyerror(std::shared_ptr<CompUnits>, const char*);
}

%code {
    #include <vector>
    #include <string>
    #include <iostream>

    int yylex(void);
    extern int yylineno;

    template<typename T>
    using sp = std::shared_ptr<T>;

    template <typename P, typename T>
    P* ptr2variant(T *ptr) {
        auto rt = new P(*ptr);
        delete ptr;
        return rt;
    }
}

%union {
    int int_val;
    float float_val;
    char* str_val;
    ASTType type;
    CompUnit *comp_unit;
    Decl* decl;
    VarDefs *var_defs;
    VarDef *var_def;
    InitVal *init_val;
    ArrayInitVal *init_vals;
    Dims *dims;
    FuncDef *func_def;
    FuncFParams *fparams;
    FuncFParam *fparam;
    BlockItems *block_items;
    BlockItem *block_item;
    Stmt *stmt;
    Exp *exp;
    Cond *cond;
    LVal *lval;
    Number *number;
    FuncRParams *rparams;
}

%type
    <comp_unit> CompUnit
    <decl> Decl ConstDecl VarDecl
    <var_defs> ConstDefs VarDefs
    <var_def> ConstDef VarDef
    <init_val> ConstInitVal InitVal
    <init_vals> ConstInitVals InitVals
    <type> BType FuncType
    <dims> Dims FuncDims
    <func_def> FuncDef
    <fparams> FuncFParams
    <fparam> FuncFParam
    <block_items> Block BlockItems
    <block_item> BlockItem
    <stmt> Stmt
    <exp> Exp PrimaryExp UnaryExp MulExp AddExp RelExp EqExp  ConstExp
    <cond> Cond LAndExp LOrExp
    <lval> LVal
    <number> Number
    <rparams> FuncRParams

%define api.token.prefix {TK_}

%token
    <str_val>
    IDENT
    <int_val>
    INT_CONST
    <float_val>
    FLOAT_CONST
    <?>
    CONST "const"
    INT "int"
    FLOAT "float"
    VOID "void"
    IF "if"
    ELSE "else"
    WHILE "while"
    BREAK "break"
    CONTINUE "continue"
    RETURN "return"
    GE ">="
    LE "<="
    EQ "=="
    NE "!="
    AND "&&"
    OR "||"

/* prevent shift/reduce conflict caused by optional "else" */
%right ELSE ')'

/* prevent reduce/reduce conflict caused by look-ahead(1) */
%glr-parser
%expect-rr 2

%%
CompUnits:
    CompUnit { comp_units->push_back(sp<CompUnit>($1)); }
    | CompUnits CompUnit { comp_units->push_back(sp<CompUnit>($2)); }

CompUnit:
    Decl { $$ = ptr2variant<CompUnit>($1); }
    | FuncDef {  $$ = ptr2variant<CompUnit>($1); }

Decl:
    ConstDecl { $$ = $1; }
    | VarDecl { $$ = $1; }

ConstDecl:
    "const" BType ConstDefs ';' { $$ = new Decl{Decl::CONST, $2, sp<VarDefs>($3)}; }

VarDecl:
    BType VarDefs ';' { $$ = new Decl{Decl::VAR, $1, sp<VarDefs>($2)}; }

ConstDefs:
    ConstDef { $$ = new VarDefs{sp<VarDef>($1)}; }
    | ConstDefs ',' ConstDef { $$ = $1; $$->push_back(sp<VarDef>($3)); }

VarDefs:
    VarDef { $$ = new VarDefs{sp<VarDef>($1)}; }
    | VarDefs ',' VarDef { $$ = $1; $$->push_back(sp<VarDef>($3)); }

ConstDef:
    IDENT Dims '=' ConstInitVal { $$ = new VarDef{Ident($1), sp<Dims>($2), sp<InitVal>($4)}; }

VarDef:
    IDENT Dims { $$ = new VarDef{Ident($1), sp<Dims>($2), nullptr}; }
    | IDENT Dims '=' InitVal { $$ = new VarDef{Ident($1), sp<Dims>($2), sp<InitVal>($4)}; }

ConstInitVal:
    ConstExp { $$ = ptr2variant<InitVal>($1); }
    | '{' '}' { $$ = new InitVal(ArrayInitVal{{}}); }
    | '{' ConstInitVals '}' { $$ = ptr2variant<InitVal>($2); }

ConstInitVals:
    ConstInitVal { $$ = new ArrayInitVal{{sp<InitVal>($1)}};  }
    | ConstInitVals ',' ConstInitVal { $$ = $1; $$->items.push_back(sp<InitVal>($3)); }

InitVal:
    Exp { $$ = ptr2variant<InitVal>($1); }
    | '{' '}' { $$ = new InitVal(ArrayInitVal{{}}); }
    | '{' InitVals '}' { $$ = ptr2variant<InitVal>($2); }

InitVals:
    InitVal { $$ = new ArrayInitVal{{sp<InitVal>($1)}}; }
    | InitVals ',' InitVal { $$ = $1; $$->items.push_back(sp<InitVal>($3)); }

BType:
    "int" { $$ = ASTType::INT; }
    | "float" { $$ = ASTType::FLOAT; }

FuncType:
    "void" { $$ = ASTType::VOID; }
    | "int" { $$ = ASTType::INT; }
    | "float" { $$ = ASTType::FLOAT; }

Dims:
    %empty { $$ = new Dims{}; }
    | Dims '[' ConstExp ']' { $$ = $1; $$->push_back(sp<Exp>($3)); }

FuncDims:
    '[' ']' { $$ = new Dims{nullptr}; }
    | FuncDims '[' Exp ']' { $$ = $1; $$->push_back(sp<Exp>($3)); }

FuncDef:
    FuncType IDENT '(' ')' Block { $$ = new FuncDef{$1, Ident($2), std::make_shared<FuncFParams>(), sp<BlockItems>($5)}; }
    | FuncType IDENT '(' FuncFParams ')' Block { $$ = new FuncDef{$1, Ident($2), sp<FuncFParams>($4), sp<BlockItems>($6)}; }

FuncFParams:
    FuncFParam { $$ = new FuncFParams{sp<FuncFParam>($1)}; }
    | FuncFParams ',' FuncFParam { $$ = $1; $$->push_back(sp<FuncFParam>($3)); }

FuncFParam:
    BType IDENT { $$ = new FuncFParam{$1, Ident($2), std::make_shared<Dims>()}; }
    | BType IDENT FuncDims { $$ = new FuncFParam{$1, Ident($2), sp<Dims>($3)}; }

Block:
    '{' BlockItems '}' { $$ = $2; }

BlockItems:
    %empty { $$ = new BlockItems{}; }
    | BlockItems BlockItem { $$ = $1; $$->push_back(sp<BlockItem>($2)); }

BlockItem:
    Decl { $$ = ptr2variant<BlockItem>($1); }
    | Stmt { $$ = ptr2variant<BlockItem>($1); }

Stmt:
    LVal '=' Exp ';' { $$ = new Stmt(AssignStmt{sp<LVal>($1), sp<Exp>($3)}); }
    | Exp ';' { $$ = new Stmt(ExpStmt{sp<Exp>($1)}); }
    | ';' { $$ = new Stmt(ExpStmt{nullptr}); }
    | Block { $$ = new Stmt(BlockStmt{sp<BlockItems>($1)}); }
    | "if" '(' Cond ')' Stmt { $$ = new Stmt(IfStmt{sp<Cond>($3), sp<Stmt>($5), nullptr}); }
    | "if" '(' Cond ')' Stmt "else" Stmt { $$ = new Stmt(IfStmt{sp<Cond>($3), sp<Stmt>($5), sp<Stmt>($7)}); }
    | "while" '(' Cond ')' Stmt { $$ = new Stmt(WhileStmt{sp<Cond>($3), sp<Stmt>($5)}); }
    | "break" ';' { $$ = new Stmt(ControlStmt{ControlStmt::BREAK}); }
    | "continue" ';' { $$ = new Stmt(ControlStmt{ControlStmt::CONTINUE}); }
    | "return" ';' { $$ = new Stmt(ReturnStmt{nullptr}); }
    | "return" Exp ';' { $$ = new Stmt(ReturnStmt{sp<Exp>($2)}); }

Exp:
    AddExp { $$ = $1; }

Cond:
    LOrExp { $$ = $1; }

LVal:
    IDENT { $$ = new LVal(Ident($1)); }
    | LVal '[' Exp ']' { $$ = new LVal(Index{sp<LVal>($1), sp<Exp>($3)}); }

PrimaryExp:
    '(' Exp ')' { $$ = $2; }
    | LVal { $$ = new Exp(LValExp{sp<LVal>($1)}); }
    | Number { $$ = ptr2variant<Exp>($1); }

Number:
    INT_CONST { $$ = new Number($1); }
    | FLOAT_CONST { $$ = new Number($1); }

UnaryExp:
    PrimaryExp { $$ = $1; }
    | IDENT '(' ')' { $$ = new Exp(CallExp{Ident($1), std::make_shared<FuncRParams>()}); }
    | IDENT '(' FuncRParams ')' { $$ = new Exp(CallExp{Ident($1), sp<FuncRParams>($3)}); }
    | '+' UnaryExp { $$ = new Exp(UnaryExp{UnaryExp::ADD, sp<Exp>($2)}); }
    | '-' UnaryExp { $$ = new Exp(UnaryExp{UnaryExp::SUB, sp<Exp>($2)}); }
    | '!' UnaryExp { $$ = new Exp(UnaryExp{UnaryExp::NOT, sp<Exp>($2)}); }

FuncRParams:
    Exp { $$ = new FuncRParams{}; }
    | FuncRParams ',' Exp { $$ = $1; $$->push_back(sp<Exp>($3)); }

MulExp:
    UnaryExp { $$ = $1; }
    | MulExp '*' UnaryExp { $$ = new Exp(BinaryExp{sp<Exp>($1), BinaryExp::MULT, sp<Exp>($3)}); }
    | MulExp '/' UnaryExp { $$ = new Exp(BinaryExp{sp<Exp>($1), BinaryExp::DIV, sp<Exp>($3)}); }
    | MulExp '%' UnaryExp { $$ = new Exp(BinaryExp{sp<Exp>($1), BinaryExp::MOD, sp<Exp>($3)}); }

AddExp:
    MulExp { $$ = $1; }
    | AddExp '+' MulExp { $$ = new Exp(BinaryExp{sp<Exp>($1), BinaryExp::ADD, sp<Exp>($3)}); }
    | AddExp '-' MulExp { $$ = new Exp(BinaryExp{sp<Exp>($1), BinaryExp::SUB, sp<Exp>($3)}); }

RelExp:
    AddExp { $$ = $1; }
    | RelExp '<' AddExp { $$ = new Exp(CompareExp{sp<Exp>($1), CompareExp::LT, sp<Exp>($3)}); }
    | RelExp '>' AddExp { $$ = new Exp(CompareExp{sp<Exp>($1), CompareExp::GT, sp<Exp>($3)}); }
    | RelExp "<=" AddExp { $$ = new Exp(CompareExp{sp<Exp>($1), CompareExp::LE, sp<Exp>($3)}); }
    | RelExp ">=" AddExp { $$ = new Exp(CompareExp{sp<Exp>($1), CompareExp::GE, sp<Exp>($3)}); }

EqExp:
    RelExp { $$ = $1; }
    | EqExp "==" RelExp { $$ = new Exp(CompareExp{sp<Exp>($1), CompareExp::EQ, sp<Exp>($3)}); }
    | EqExp "!=" RelExp { $$ = new Exp(CompareExp{sp<Exp>($1), CompareExp::NE, sp<Exp>($3)}); }

LAndExp:
    EqExp { $$ = ptr2variant<Cond>($1); }
    | LAndExp "&&" EqExp { $$ = new Cond(LogicalExp{sp<Cond>($1), LogicalExp::AND, sp<Cond>(ptr2variant<Cond>($3))}); }

LOrExp:
    LAndExp { $$ = $1; }
    | LOrExp "||" LAndExp { $$ = new Cond(LogicalExp{sp<Cond>($1), LogicalExp::OR, sp<Cond>($3)}); }

ConstExp:
    AddExp { $$ = $1; }
%%

void yyerror(std::shared_ptr<CompUnits> comp_units, const char *s) {
    std::cerr << yylineno << ": " << s << std::endl;
}
