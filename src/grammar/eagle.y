%{
    #include <stdlib.h>
    #include "compiler/ast.h"

    extern int yylex();
    extern int yyerror(char *);
    
    AST *ast_root = NULL;
%}

%union {
    int token;
    char *string;
    AST *node;
}

%token <string> TIDENTIFIER TINT TDOUBLE TTYPE
%token <token> TPLUS TMINUS
%token <token> TLPAREN TRPAREN TLBRACE TRBRACE
%token <token> TFUNC TRETURN TPUTS
%token <token> TCOLON TSEMI TNEWLINE

%type <node> program declarations declaration statements statement block funcdecl expression

%left TPLUS;

%start program

%%

program             : declarations { ast_root = $1; };

declarations        : declaration { $$ = $1; }
                    | declaration declarations { ast_append($2, $1); $$ = $2; };

declaration         : funcdecl { $$ = $1; };

funcdecl            : TFUNC TIDENTIFIER TLPAREN TRPAREN TCOLON TTYPE block { $$ = ast_make_func_decl($6, $2, $7); };

block               : TLBRACE statements TRBRACE { $$ = $2; };

statements          : statement { if($1) $$ = $1; else $$ = ast_make(); }
                    | statement statements { if($1) $1->next = $2; $$ = $1 ? $1 : $2; };

statement           : expression TSEMI { $$ = $1; }
                    | expression TNEWLINE { $$ = $1; }
                    | TRETURN expression TSEMI { $$ = ast_make_unary($2, 'r'); }
                    | TRETURN expression TNEWLINE { $$ = ast_make_unary($2, 'r'); }
                    | TPUTS expression TSEMI { $$ = ast_make_unary($2, 'p'); }
                    | TPUTS expression TNEWLINE { $$ = ast_make_unary($2, 'p'); }
                    | TNEWLINE { $$ = NULL; };

expression          : TINT { $$ = ast_make_int32($1); }
                    | TDOUBLE { $$ = ast_make_double($1); }
                    | expression TPLUS expression { $$ = ast_make_binary($1, $3, '+'); };

%%
