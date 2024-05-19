/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Skeleton interface for Bison GLR parsers in C

   Copyright (C) 2002-2015, 2018-2021 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_YY_INCLUDE_PARSER_H_INCLUDED
# define YY_YY_INCLUDE_PARSER_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif
#if YYDEBUG
extern int yydebug;
#endif
/* "%code requires" blocks.  */
#line 6 "parser.y"

    #include "ast.h"
    #include <memory>
    void yyerror(std::shared_ptr<CompUnits>, const char*);

#line 50 "include/parser.h"

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    TK_YYEMPTY = -2,
    TK_YYEOF = 0,                  /* "end of file"  */
    TK_YYerror = 256,              /* error  */
    TK_YYUNDEF = 257,              /* "invalid token"  */
    TK_IDENT = 258,                /* IDENT  */
    TK_INT_CONST = 259,            /* INT_CONST  */
    TK_FLOAT_CONST = 260,          /* FLOAT_CONST  */
    TK_CONST = 261,                /* "const"  */
    TK_INT = 262,                  /* "int"  */
    TK_FLOAT = 263,                /* "float"  */
    TK_VOID = 264,                 /* "void"  */
    TK_IF = 265,                   /* "if"  */
    TK_ELSE = 266,                 /* "else"  */
    TK_WHILE = 267,                /* "while"  */
    TK_BREAK = 268,                /* "break"  */
    TK_CONTINUE = 269,             /* "continue"  */
    TK_RETURN = 270,               /* "return"  */
    TK_GE = 271,                   /* ">="  */
    TK_LE = 272,                   /* "<="  */
    TK_EQ = 273,                   /* "=="  */
    TK_NE = 274,                   /* "!="  */
    TK_AND = 275,                  /* "&&"  */
    TK_OR = 276                    /* "||"  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 32 "parser.y"

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

#line 113 "include/parser.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (std::shared_ptr<CompUnits> comp_units);

#endif /* !YY_YY_INCLUDE_PARSER_H_INCLUDED  */
