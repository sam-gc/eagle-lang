%{
    #include <stdio.h>
    #include <string.h>
    #include "compiler/ast.h"
    #include "eagle.tab.h"
    
    #define SET(t) (yylval.token = t)
    #define SAVE_TOKEN (yylval.string = strdup(yytext))
    #define DISCARD_NL (save_newline = 0)
    #define SAVE_NL (save_newline = 1)

int translate = 0;
    int save_newline = 0;
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

"\n"          { if(save_newline) {save_newline=0; return TSEMI;} /*else printf("IGNORING! %d\n", yylineno);*/ }

{integer}     { SAVE_NL; SAVE_TOKEN; return TINT; }
{real}      { SAVE_NL; SAVE_TOKEN; return TDOUBLE; }
"+"         DISCARD_NL; return SET(TPLUS);
"-"         DISCARD_NL; return SET(TMINUS);
"*"         DISCARD_NL; return SET(TMUL);
"/"         DISCARD_NL; return SET(TDIV);
"="         DISCARD_NL; return SET(TEQUALS);
"func"      DISCARD_NL; return SET(TFUNC);
":"         DISCARD_NL; return SET(TCOLON);
","         DISCARD_NL; return SET(TCOMMA);
";"      DISCARD_NL; return SET(TSEMI);
"("         DISCARD_NL; return SET(TLPAREN);
")"         SAVE_NL; return SET(TRPAREN);
{lbrace}    return SET(TLBRACE);
"}"         DISCARD_NL; return SET(TRBRACE);
"return"    DISCARD_NL; return SET(TRETURN);
"puts"      DISCARD_NL; return SET(TPUTS);
"extern"    DISCARD_NL; return SET(TEXTERN);
"if"        DISCARD_NL; return SET(TIF);
"elif"      DISCARD_NL; return SET(TELIF);
"else"      DISCARD_NL; return SET(TELSE);
"int"       { SAVE_NL; SAVE_TOKEN; return TTYPE; }
"long"      { SAVE_NL; SAVE_TOKEN; return TTYPE; }
"double"    { SAVE_NL; SAVE_TOKEN; return TTYPE; }
"void"      { SAVE_NL; SAVE_TOKEN; return TTYPE; }
{alpha}       { SAVE_NL; SAVE_TOKEN; return TIDENTIFIER; }
%%
