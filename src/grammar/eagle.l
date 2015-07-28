%{
    #include <stdio.h>
    #include <string.h>
    #include "compiler/ast.h"
    #include "eagle.tab.h"
    
    #define SET(t) (yylval.token = t)
    #define SAVE_TOKEN (yylval.string = strdup(yytext))
%}

%option noyywrap
%option yylineno
%option noinput
%option nounput

white [ \t]+
whitechar [ \t]
digit [0-9]
lalpha [a-z]
alpha [a-zA-Z_$]+
integer {digit}+
exponent [eE][+-]?{integer}
real {integer}("."{digit}*){exponent}?
nvar {alpha}({alpha}|{digit})*
strlit (\"(\\.|[^"])*\"|'(\\.|[^'])*')
comment (\#|(\-\-)).*(\n)
lbrace ("{"|"{"{whitechar}*"\n"|"\n"{whitechar}*"{")
rbrace ("}"|"}"{whitechar}*"\n")
func ("func"|"func"{whitechar}*"\n")
semi (";"|";"{whitechar}*"\n")

%%

{white}       ;

"\n"         return SET(TNEWLINE);

{integer}     { SAVE_TOKEN; return TINT; }
{real}      {SAVE_TOKEN; return TDOUBLE; }
"+"         return SET(TPLUS);
"-"         return SET(TMINUS);
"*"         return SET(TMUL);
"/"         return SET(TDIV);
"="         return SET(TEQUALS);
{func}      return SET(TFUNC);
":"         return SET(TCOLON);
","         return SET(TCOMMA);
{semi}      return SET(TSEMI);
"("         return SET(TLPAREN);
")"         return SET(TRPAREN);
{lbrace}    return SET(TLBRACE);
"}"    return SET(TRBRACE);
"return"    return SET(TRETURN);
"puts"      return SET(TPUTS);
"extern"    return SET(TEXTERN);
"int"       { SAVE_TOKEN; return TTYPE; }
"long"      { SAVE_TOKEN; return TTYPE; }
"double"    { SAVE_TOKEN; return TTYPE; }
"void"      { SAVE_TOKEN; return TTYPE; }
{alpha}       { SAVE_TOKEN; return TIDENTIFIER; }
%%
