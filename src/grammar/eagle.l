%{
    #include <stdio.h>
    #include <string.h>
    #include "compiler/ast.h"
    #include "core/utils.h"
    #include "core/multibuffer.h"
    #include "eagle.tab.h"
    
    #define SET(t) (yylval.token = t)
    #define SAVE_TOKEN yylval.string = strdup(yytext); utl_register_memory(yylval.string)
    #define DISCARD_NL (save_newline = 0)
    #define SAVE_NL (save_newline = 1)
    #define OVERRIDE (override = 1)
    #define OVEROVERIDE (override = 0)

    int save_newline = 0;
    int override = 0;
    int in_interface = 0;
    int skip_type_check = 0;
    extern multibuffer *ymultibuffer;

    #define YY_INPUT(buf, result, max_size) result = mb_buffer(ymultibuffer, buf, max_size)
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
strlit \"(\\.|[^"])*\"
comment (\#|(\-\-)).*(\n)
lbrace ("{"|"{"{whitechar}*"\n"|"\n"{whitechar}*"{")
rbrace ("}"|"}"{whitechar}*"\n")
func ("func"|"func"{whitechar}*"\n")
semi (";"|";"{whitechar}*"\n")
cstr '(\\.|[^'])*'

%%

{white}       ;

"\n"          { if(save_newline || override) {save_newline = override = 0; return TSEMI;} override = 0; /*else printf("IGNORING! %d\n", yylineno);*/ }

{integer}     { SAVE_NL; SAVE_TOKEN; return TINT; }
{real}      { SAVE_NL; SAVE_TOKEN; return TDOUBLE; }
"+"         DISCARD_NL; return SET(TPLUS);
"+="        DISCARD_NL; return SET(TPLUSE);
"-"         DISCARD_NL; return SET(TMINUS);
"-="        DISCARD_NL; return SET(TMINUSE);
"*"         DISCARD_NL; return SET(TMUL);
"*="        DISCARD_NL; return SET(TMULE);
"/"         DISCARD_NL; return SET(TDIV);
"/="        DISCARD_NL; return SET(TDIVE);
"^"         SAVE_NL; return SET(TPOW);
">"         DISCARD_NL; return SET(TGT);
">="        DISCARD_NL; return SET(TGTE);
"=="        DISCARD_NL; return SET(TEQ);
"<"         DISCARD_NL; return SET(TLT);
"<="        DISCARD_NL; return SET(TLTE);
"!="        DISCARD_NL; return SET(TNE);
"!"         DISCARD_NL; return SET(TNOT);
"="         DISCARD_NL; return SET(TEQUALS);
"&&"        DISCARD_NL; return SET(TLOGAND);
"||"        DISCARD_NL; return SET(TLOGOR);
"|"         DISCARD_NL; return SET(TOR);
"func"      DISCARD_NL; in_interface && OVERRIDE; return SET(TFUNC);
"gen"       DISCARD_NL; return SET(TGEN);
":"         DISCARD_NL; return SET(TCOLON);
","         DISCARD_NL; return SET(TCOMMA);
"."         DISCARD_NL; return SET(TDOT);
"->"        DISCARD_NL; return SET(TARROW);
";"      DISCARD_NL; return SET(TSEMI);
"&"         DISCARD_NL; return SET(TAMP);
"@"         DISCARD_NL; return SET(TAT);
"("         DISCARD_NL; return SET(TLPAREN);
")"         SAVE_NL; return SET(TRPAREN);
{lbrace}    OVEROVERIDE; return SET(TLBRACE);
"}"         SAVE_NL; in_interface = 0; return SET(TRBRACE);
"["         DISCARD_NL; return SET(TLBRACKET);
"]"         SAVE_NL; return SET(TRBRACKET);
"return"    SAVE_NL; return SET(TRETURN);
"yield"     DISCARD_NL; return SET(TYIELD);
"break"     SAVE_NL; return SET(TBREAK);
"continue"  SAVE_NL; return SET(TCONTINUE);
"struct"    DISCARD_NL; return SET(TSTRUCT);
"class"     DISCARD_NL; return SET(TCLASS);
"interface" DISCARD_NL; in_interface = 1; return SET(TINTERFACE);
"puts"      DISCARD_NL; return SET(TPUTS);
"extern"    DISCARD_NL; OVERRIDE; return SET(TEXTERN);
"sizeof"    DISCARD_NL; return SET(TSIZEOF);
"countof"   DISCARD_NL; return SET(TCOUNTOF);
"counted"   DISCARD_NL; return SET(TCOUNTED);
"..."       DISCARD_NL; return SET(TELLIPSES);
"weak"      DISCARD_NL; return SET(TWEAK);
"unwrap"    DISCARD_NL; return SET(TUNWRAP);
"touch"     DISCARD_NL; return SET(TTOUCH);
"new"       DISCARD_NL; return SET(TNEW);
"if"        DISCARD_NL; return SET(TIF);
"for"       DISCARD_NL; return SET(TFOR);
"in"        DISCARD_NL; return SET(TIN);
"elif"      DISCARD_NL; return SET(TELIF);
"else"      DISCARD_NL; return SET(TELSE);
"yes"       SAVE_NL; return SET(TYES);
"no"        SAVE_NL; return SET(TNO);
"nil"       SAVE_NL; return SET(TNIL);
"var"       DISCARD_NL; return SET(TVAR);
"bool"      SAVE_NL; SAVE_TOKEN; return TTYPE;
"byte"      SAVE_NL; SAVE_TOKEN; return TTYPE; 
"short"     SAVE_NL; SAVE_TOKEN; return TTYPE;
"int"       { SAVE_NL; SAVE_TOKEN; return TTYPE; }
"long"      { SAVE_NL; SAVE_TOKEN; return TTYPE; }
"double"    { SAVE_NL; SAVE_TOKEN; return TTYPE; }
"any"       { SAVE_NL; SAVE_TOKEN; return TTYPE; }
{alpha}       { SAVE_NL; SAVE_TOKEN; if(skip_type_check) return TIDENTIFIER; else return ty_is_name(yytext) ? TTYPE : TIDENTIFIER; }
{cstr}      SAVE_NL; yylval.string = utl_gen_escaped_string((char *)yytext, yylineno); return TCSTR;
%%
