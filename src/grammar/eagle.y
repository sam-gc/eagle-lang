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
%token <token> TPLUS TMINUS TEQUALS
%token <token> TLPAREN TRPAREN TLBRACE TRBRACE
%token <token> TFUNC TRETURN TPUTS
%token <token> TCOLON TSEMI TNEWLINE TCOMMA

%type <node> program declarations declaration statements statement block funcdecl expression
%type <node> variabledecl vardecllist

%right TEQUALS;
%left TPLUS;

%start program

%%

program             : declarations { ast_root = $1; };

declarations        : declaration { $$ = $1; }
                    | declaration declarations { $1->next = $2; $$ = $1; };

declaration         : funcdecl { $$ = $1; };

funcdecl            : TFUNC TIDENTIFIER TLPAREN TRPAREN TCOLON TTYPE block { $$ = ast_make_func_decl($6, $2, $7, NULL); }
                    | TFUNC TIDENTIFIER TLPAREN vardecllist TRPAREN TCOLON TTYPE block { $$ = ast_make_func_decl($7, $2, $8, $4); };

vardecllist         : variabledecl { $$ = $1; }
                    | variabledecl TCOMMA vardecllist { $1->next = $3; $$ = $1; };

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

variabledecl        : TTYPE TIDENTIFIER { $$ = ast_make_var_decl($1, $2); };

expression          : TINT { $$ = ast_make_int32($1); }
                    | TDOUBLE { $$ = ast_make_double($1); }
                    | expression TPLUS expression { $$ = ast_make_binary($1, $3, '+'); }
                    | variabledecl { $$ = $1; }
                    | variabledecl TEQUALS expression { $$ = ast_make_binary($1, $3, '='); }
                    | TIDENTIFIER TEQUALS expression { $$ = ast_make_binary(ast_make_identifier($1), $3, '='); }
                    | TIDENTIFIER { $$ = ast_make_identifier($1); };

%%
