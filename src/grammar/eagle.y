%{
    #include <stdlib.h>
    #include "compiler/ast.h"

    extern int yylex();
    extern int yyerror(const char *);
    
    AST *ast_root = NULL;
%}

%error-verbose
%expect 4

%union {
    int token;
    char *string;
    AST *node;
}

%token <string> TIDENTIFIER TINT TDOUBLE TTYPE TCOUNTED TNEW
%token <token> TPLUS TMINUS TEQUALS TMUL TDIV TGT TLT TEQ TNE TGTE TLTE TNOT
%token <token> TLPAREN TRPAREN TLBRACE TRBRACE TLBRACKET TRBRACKET
%token <token> TFUNC TRETURN TPUTS TEXTERN TIF TELSE TELIF TSIZEOF TCOUNTOF
%token <token> TCOLON TSEMI TNEWLINE TCOMMA TAMP TAT
%token <token> TYES TNO TNIL
%type <node> program declarations declaration statements statement block funcdecl ifstatement
%type <node> variabledecl vardecllist funccall calllist funcident funcsident externdecl typelist type
%type <node> elifstatement elifblock elsestatement singif 
%type <node> expr singexpr binexpr unexpr ounexpr

%nonassoc TNEW;
%right TEQUALS;
%left TNOT;
%left TEQ TNE TLT TGT TLTE TGTE
%left TPLUS TMINUS;
%left TMUL TDIV;
%left TAT TAMP;
%nonassoc TSIZEOF TCOUNTOF;
%right TLBRACKET TRBRACKET;
%right "then" TIF TELSE TELIF %nonassoc TLPAREN;

%start program

%%

program             : declarations { ast_root = $1; };

declarations        : declaration { if($1) $$ = $1; else $$ = ast_make(); }
                    | declaration declarations { if($1) $1->next = $2; $$ = $1 ? $1 : $2; };

declaration         : externdecl TSEMI { $$ = $1; }
                    | funcdecl { $$ = $1; };

type                : TTYPE { $$ = ast_make_type($1); }
                    | type TMUL { $$ = ast_make_pointer($1); }
                    | type TLBRACKET TRBRACKET { $$ = ast_make_array($1, -1); }
                    | type TLBRACKET TINT TRBRACKET { $$ = ast_make_array($1, atoi($3)); }
                    ;

funcdecl            : funcident block { ((ASTFuncDecl *)$1)->body = $2; $$ = $1; };

externdecl          : TEXTERN funcident { $$ = $2; }
                    | TEXTERN funcsident { $$ = $2; };

funcident           : TFUNC TIDENTIFIER TLPAREN TRPAREN TCOLON type { $$ = ast_make_func_decl($6, $2, NULL, NULL); }
                    | TFUNC TIDENTIFIER TLPAREN vardecllist TRPAREN TCOLON type { $$ = ast_make_func_decl($7, $2, NULL, $4); };

funcsident          : TFUNC TIDENTIFIER TLPAREN typelist TRPAREN TCOLON type { $$ = ast_make_func_decl($7, $2, NULL, $4); };

typelist            : type { $$ = ast_make_var_decl($1, NULL); }
                    | type TCOMMA typelist { AST *a = ast_make_var_decl($1, NULL); a->next = $3; $$ = a; };

vardecllist         : variabledecl { $$ = $1; }
                    | variabledecl TCOMMA vardecllist { $1->next = $3; $$ = $1; };

block               : TLBRACE statements TRBRACE { $$ = $2; };

statements          : statement { if($1) $$ = $1; else $$ = ast_make(); }
                    | statement statements { if($1) $1->next = $2; $$ = $1 ? $1 : $2; };

statement           : expr TSEMI { $$ = $1; }
                    | TRETURN expr TSEMI { $$ = ast_make_unary($2, 'r'); }
                    | TPUTS expr TSEMI { $$ = ast_make_unary($2, 'p'); }
                    | TSEMI { $$ = NULL; }
                    | ifstatement { $$ = $1; };

singif              : TIF expr TSEMI statement { $$ = ast_make_if($2, $4); }
                    | TIF expr block { $$ = ast_make_if($2, $3); };

elsestatement       : TELSE statement { $$ = ast_make_if(NULL, $2); }
                    | TELSE block { $$ = ast_make_if(NULL, $2); };

elifstatement       : TELIF expr TSEMI statement { $$ = ast_make_if($2, $4); }
                    | TELIF expr block { $$ = ast_make_if($2, $3); };

elifblock           : elifstatement { $$ = $1; }
                    | elifblock elifstatement { $$ = $1; ast_add_if($1, $2); };

ifstatement         : singif { $$ = $1; }
                    | singif elsestatement { $$ = $1; ast_add_if($$, $2); }
                    | singif elifblock { $$ = $1; ast_add_if($1, $2); }
                    | singif elifblock elsestatement { $$ = $1; ast_add_if($$, $2); ast_add_if($$, $3); };

variabledecl        : type TIDENTIFIER { $$ = ast_make_var_decl($1, $2); }
                    | TCOUNTED type TIDENTIFIER { $$ = ast_make_var_decl($2, $3); ast_set_counted($$); };

funccall            : ounexpr TLPAREN TRPAREN { $$ = ast_make_func_call($1, NULL); }
                    | ounexpr TLPAREN calllist TRPAREN { $$ = ast_make_func_call($1, $3); };

calllist            : expr { $$ = $1; }
                    | expr TCOMMA calllist { $1->next = $3; $$ = $1; };

expr                : binexpr { $$ = $1; }
                    | unexpr { $$ = $1; }
                    | singexpr { $$ = $1; }
                    | variabledecl { $$ = $1; }
                    | type TAT ounexpr { $$ = ast_make_cast($1, $3); }
                    | TNEW type { $$ = ast_make_allocater('n', $2); }
                    ;

binexpr             : expr TPLUS expr { $$ = ast_make_binary($1, $3, '+'); }
                    | expr TEQUALS expr { $$ = ast_make_binary($1, $3, '='); }
                    | expr TMINUS expr { $$ = ast_make_binary($1, $3, '-'); }
                    | expr TMUL expr { $$ = ast_make_binary($1, $3, '*'); }
                    | expr TDIV expr { $$ = ast_make_binary($1, $3, '/'); }
                    | expr TEQ expr { $$ = ast_make_binary($1, $3, 'e'); }
                    | expr TGT expr { $$ = ast_make_binary($1, $3, 'g'); }
                    | expr TGTE expr { $$ = ast_make_binary($1, $3, 'G'); }
                    | expr TLT expr { $$ = ast_make_binary($1, $3, 'l'); }
                    | expr TLTE expr { $$ = ast_make_binary($1, $3, 'L'); }
                    | expr TNE expr { $$ = ast_make_binary($1, $3, 'n'); }
                    ;

unexpr              : TMUL ounexpr { $$ = ast_make_unary($2, '*'); }
                    | TAMP ounexpr { $$ = ast_make_unary($2, '&'); }
                    | TNOT ounexpr { $$ = ast_make_unary($2, '!'); }
                    | TSIZEOF ounexpr { $$ = ast_make_unary($2, 's'); }
                    | TCOUNTOF ounexpr { $$ = ast_make_unary($2, 'c'); }
                    | ounexpr TLBRACKET expr TRBRACKET { $$ = ast_make_binary($1, $3, '['); }
                    ; 

ounexpr             : singexpr { $$ = $1; }
                    | unexpr { $$ = $1; }
                    ;

singexpr            : TINT { $$ = ast_make_int32($1); }
                    | TDOUBLE { $$ = ast_make_double($1); }
                    | TYES { $$ = ast_make_bool(1); }
                    | TNO { $$ = ast_make_bool(0); }
                    | TNIL { $$ = ast_make_nil(); }
                    | TIDENTIFIER { $$ = ast_make_identifier($1); }
                    | TLPAREN expr TRPAREN { $$ = $2; }
                    | funccall { $$ = $1; }
                    ;

%%
