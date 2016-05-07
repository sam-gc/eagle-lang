%{
/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
 
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
extern Multibuffer *ymultibuffer;
extern int pipe_is_type();

extern int yyerror(const char *);
#define YY_INPUT(buf, result, max_size) if(skip_type_check) result = fread( buf, 1, max_size, yyin ); else result = mb_buffer(ymultibuffer, buf, max_size)

static int start_token;
static int input (void );
static void yyunput (int c,char *buf_ptr  );

void yyset_start_token(int tok)
{
    start_token = tok;
}

#define unput(c) yyunput( c, (yytext_ptr)  )
char yypeek()
{
    char tok = input();
    unput(tok);
    return tok;
}

%}

%option noyywrap
%option yylineno

reset "=== RESET ==="
white [ \t]+
whitechar [ \t]
digit [0-9]
lalpha [a-z]
alpha [a-zA-Z_$]+
integer (0x)?{digit}+
exponent [eE][+-]?{integer}
real {integer}("."{digit}*){exponent}?
nvar {alpha}({alpha}|{digit})*
strlit \"(\\.|[^"])*\"
comment (\-\-).*(\n)
import "import ".*
lbrace ("{"|"{"{whitechar}*"\n"|"\n"{whitechar}*"{")
rbrace ("}"|"}"{whitechar}*"\n")
func ("func"|"func"{whitechar}*"\n")
semi (";"|";"{whitechar}*"\n")
cstr '(\\.|[^'])*'
charlit `[^`]`

%%
%{

if(start_token)
{
    int temp = start_token;
    start_token = 0;
    return temp;
}

%}

{white}       ;
"\n"          { if(save_newline || override) {save_newline = override = 0; return TSEMI;} override = 0; /*else printf("IGNORING! %d\n", yylineno);*/ }

{integer}     { SAVE_NL; SAVE_TOKEN; return TINT; }
{charlit}   { SAVE_NL; SAVE_TOKEN; return TCHARLIT; }
{real}      { SAVE_NL; SAVE_TOKEN; return TDOUBLE; }
{import}    { SAVE_TOKEN; return TIMPORT; }
{reset}     yylineno = 1;
"export"    { DISCARD_NL; return TEXPORT; }
">>"        DISCARD_NL; return SET(TRSHIFT);
">>="       DISCARD_NL; return SET(TRSHIFTE);
"<<"        DISCARD_NL; return SET(TLSHIFT);
"<<="       DISCARD_NL; return SET(TLSHIFTE);
"+"         DISCARD_NL; return SET(TPLUS);
"+="        DISCARD_NL; return SET(TPLUSE);
"~"         DISCARD_NL; return SET(TTILDE);
"-"         DISCARD_NL; return SET(TMINUS);
"-="        DISCARD_NL; return SET(TMINUSE);
"*"         SAVE_NL; return SET(TMUL);
"*="        DISCARD_NL; return SET(TMULE);
"/"         DISCARD_NL; return SET(TDIV);
"/="        DISCARD_NL; return SET(TDIVE);
"%"         DISCARD_NL; return SET(TMOD);
"%="        DISCARD_NL; return SET(TMODE);
"^"         SAVE_NL; return SET(TPOW);
"^="        SAVE_NL; return SET(TPOWE);
">"         DISCARD_NL; return SET(TGT);
">="        DISCARD_NL; return SET(TGTE);
"=="        DISCARD_NL; return SET(TEQ);
"<"         DISCARD_NL; return SET(TLT);
"<="        DISCARD_NL; return SET(TLTE);
"!="        DISCARD_NL; return SET(TNE);
"!"         SAVE_NL; return SET(TNOT);
"="         DISCARD_NL; return SET(TEQUALS);
"&&"        DISCARD_NL; return SET(TLOGAND);
"&="        DISCARD_NL; return SET(TAMPE);
"||"        DISCARD_NL; return SET(TLOGOR);
"|"         DISCARD_NL; return SET(TOR);
"|="        DISCARD_NL; return SET(TORE);
"func"      DISCARD_NL; in_interface && OVERRIDE; return SET(TFUNC);
"gen"       DISCARD_NL; return SET(TGEN);
"view"      DISCARD_NL; return SET(TVIEW);
":"         DISCARD_NL; return SET(TCOLON);
"?:"        DISCARD_NL; return SET(TQUESTIONCOLON);
"?"         DISCARD_NL; return SET(TQUESTION);
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
"macro"     DISCARD_NL; return SET(TMACRO);
"typedef"   DISCARD_NL; return SET(TTYPEDEF);
"enum"      DISCARD_NL; return SET(TENUM);
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
"static"    DISCARD_NL; return SET(TSTATIC);
"..."       DISCARD_NL; return SET(TELLIPSES);
"weak"      DISCARD_NL; return SET(TWEAK);
"unwrap"    DISCARD_NL; return SET(TUNWRAP);
"touch"     DISCARD_NL; return SET(TTOUCH);
"__inc"     DISCARD_NL; return SET(T__INC);
"__dec"     DISCARD_NL; return SET(T__DEC);
"new"       DISCARD_NL; return SET(TNEW);
"if"        DISCARD_NL; return SET(TIF);
"for"       DISCARD_NL; return SET(TFOR);
"in"        DISCARD_NL; return SET(TIN);
"elif"      DISCARD_NL; return SET(TELIF);
"else"      DISCARD_NL; return SET(TELSE);
"switch"    DISCARD_NL; return SET(TSWITCH);
"case"      DISCARD_NL; return SET(TCASE);
"fallthrough" SAVE_NL; return SET(TFALLTHROUGH);
"default"   SAVE_NL; return SET(TDEFAULT);
"defer"     DISCARD_NL; return SET(TDEFER);
"yes"       SAVE_NL; return SET(TYES);
"no"        SAVE_NL; return SET(TNO);
"nil"       SAVE_NL; return SET(TNIL);
"var"       DISCARD_NL; return SET(TVAR);
"bool"      SAVE_NL; SAVE_TOKEN; return TTYPE;
"byte"      SAVE_NL; SAVE_TOKEN; return TTYPE;
"ubyte"     SAVE_NL; SAVE_TOKEN; return TTYPE;
"short"     SAVE_NL; SAVE_TOKEN; return TTYPE;
"ushort"     SAVE_NL; SAVE_TOKEN; return TTYPE;
"int"       { SAVE_NL; SAVE_TOKEN; return TTYPE; }
"uint"       { SAVE_NL; SAVE_TOKEN; return TTYPE; }
"long"      { SAVE_NL; SAVE_TOKEN; return TTYPE; }
"ulong"      { SAVE_NL; SAVE_TOKEN; return TTYPE; }
"double"    { SAVE_NL; SAVE_TOKEN; return TTYPE; }
"float"     { SAVE_NL; SAVE_TOKEN; return TTYPE; }
"any"       { SAVE_NL; SAVE_TOKEN; return TTYPE; }
{nvar}       { SAVE_NL; SAVE_TOKEN; if(skip_type_check) return TIDENTIFIER; else return ty_is_name(yytext) || pipe_is_type(yytext) ? TTYPE : TIDENTIFIER; }
{cstr}      SAVE_NL; yylval.string = utl_gen_escaped_string((char *)yytext, yylineno); return TCSTR;
<<EOF>>     {static int seen_eof; if(seen_eof) { seen_eof = 0; return 0; } else {seen_eof = 1; return TSEMI; }}
"-*"        {
// Adapted from https://www.cs.princeton.edu/~appel/modern/c/software/flex/flex.html
char c;
while(1)
{
    while((c = input()) != '*' && c != EOF);
    if(c == '*')
    {
        while((c = input()) == '*');
        if(c == '-')
            break;
    }

    if(c == EOF)
        yyerror("End of line in comment");
}
}
"--"        {
char c;
while(1)
{
    while((c = input()) != '\n' && c != EOF);
    if(c == '\n')
    {
        unput('\n');
        break;
    }

    if(c == EOF)
        yyerror("End of line in comment");
}
}
%%
