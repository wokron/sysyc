%option noyywrap yylineno

%{
    #include "parser.h"
    #include "error.h"
%}

/* ident */
digit [0-9]
ident_nondight [a-zA-Z_]
ident {ident_nondight}({digit}|{ident_nondight})*

/* decimal integer const */
nonzero_dight [1-9]
dec_int {nonzero_dight}{digit}*

/* octal integer const */
oct_digit [0-7]
oct_int 0{oct_digit}*

/* hexadecimal integer const */
hex_digit [0-9a-fA-F]
hex_prefix 0[xX]
hex_int {hex_prefix}{hex_digit}+

/* decimal float const */
digit_seq {digit}+
exp_part [eE][+-]?{digit_seq}
frac ({digit_seq}?\.{digit_seq}|{digit_seq}\.)
dec_float ({frac}{exp_part}?|{digit_seq}{exp_part})

/* hexadecimal float const */
hex_seq {hex_digit}+
bin_exp_part [pP][+-]?{digit_seq}
hex_frac ({hex_seq}?\.{hex_seq}|{hex_seq}\.)
hex_float {hex_prefix}({hex_frac}{bin_exp_part}|{hex_seq}{bin_exp_part})

/* comments */
single_line_comment "//".*
multi_line_comment [/][*][^*]*[*]+([^*/][^*]*[*]+)*[/]
comments {single_line_comment}|{multi_line_comment}

%%
{dec_int} {
    yylval.int_val = (int)strtol(yytext, NULL, 10);
    return TK_INT_CONST;
}

{oct_int} {
    yylval.int_val = (int)strtol(yytext, NULL, 8);
    return TK_INT_CONST;
}

{hex_int} {
    yylval.int_val = (int)strtol(yytext, NULL, 16);
    return TK_INT_CONST;
}

{dec_float} {
    yylval.float_val = (float)atof(yytext);
    return TK_FLOAT_CONST;
}

{hex_float} {
    yylval.float_val = (float)atof(yytext);
    return TK_FLOAT_CONST;
}

"const" {
    return TK_CONST;
}

"int" {
    return TK_INT;
}

"float" {
    return TK_FLOAT;
}

"void" {
    return TK_VOID;
}

"if" {
    return TK_IF;
}

"else" {
    return TK_ELSE;
}

"while" {
    return TK_WHILE;
}

"break" {
    return TK_BREAK;
}

"continue" {
    return TK_CONTINUE;
}

"return" {
    return TK_RETURN;
}

"<=" {
    return TK_LE;
}

">=" {
    return TK_GE;
}

"==" {
    return TK_EQ;
}

"!=" {
    return TK_NE;
}

"&&" {
    return TK_AND;
}

"||" {
    return TK_OR;
}

[,;()[\]\{\}=+\-!*/%<>] {
    return yytext[0];
}

{ident} {
    yylval.str_val = strdup(yytext);
    return TK_IDENT;
}

{comments} { }

[[:space:]]+ { }

. {
    error(yylineno, "invalid input");
}
%%
