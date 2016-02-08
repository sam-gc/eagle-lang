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

%token <string> TIDENTIFIER TINT TDOUBLE TTYPE TCOUNTED TNEW TSTRUCT TCLASS TTOUCH TCSTR TINTERFACE
%token <token> TPLUS TMINUS TEQUALS TMUL TDIV TGT TLT TEQ TNE TGTE TLTE TNOT TPOW TLOGAND TLOGOR TMOD
%token <token> TPLUSE TMINUSE TMULE TDIVE TOR
%token <token> TLPAREN TRPAREN TLBRACE TRBRACE TLBRACKET TRBRACKET
%token <token> TFUNC TRETURN TYIELD TPUTS TEXTERN TIF TELSE TELIF TSIZEOF TCOUNTOF TFOR TIN TWEAK TUNWRAP
%token <token> TBREAK TCONTINUE TVAR TGEN  TELLIPSES TVIEW
%token <token> TCOLON TSEMI TNEWLINE TCOMMA TDOT TAMP TAT TARROW
%token <token> TYES TNO TNIL TIMPORT TEXPORT
%type <node> program declarations declaration statements statement block funcdecl ifstatement
%type <node> variabledecl vardecllist funccall calllist funcident funcsident externdecl typelist type interfacetypelist
%type <node> elifstatement elifblock elsestatement singif structdecl structlist blockalt classlist classdecl interfacedecl interfacelist compositetype
%type <node> expr singexpr binexpr unexpr ounexpr forstatement clodecl genident gendecl initdecl viewdecl viewident

%nonassoc TTYPE;
%nonassoc TNEW;
%nonassoc TCOUNTED TWEAK TUNWRAP TTOUCH;
%right TPLUSE TMINUSE;
%right TMULE TDIVE;
%right TEQUALS;
%left TLOGOR;
%left TLOGAND;
%left TNOT;
%left TAMP TOR;
%left TEQ TNE TLT TGT TLTE TGTE
%left TPLUS TMINUS;
%left TMUL TDIV TMOD;
%left TAT;
%left TDOT TPOW TARROW;
%nonassoc TSIZEOF TCOUNTOF;
%right TLBRACKET TRBRACKET;
%right "then" TIF TELSE TELIF %nonassoc TLPAREN;

%start program

%%

program             : declarations { ast_root = $1; };

declarations        : declaration { if($1) $$ = $1; else $$ = ast_make(); }
                    | declaration declarations { if($1) $1->next = $2; $$ = $1 ? $1 : $2; };

declaration         : externdecl TSEMI { $$ = $1; }
                    | funcdecl { $$ = $1; }
                    | structdecl TSEMI { $$ = $1; }
                    | gendecl { $$ = $1; }
                    | classdecl TSEMI { $$ = $1; }
                    | interfacedecl TSEMI { $$ = $1; }
                    | TIMPORT { $$ = NULL; }
                    | TEXPORT { $$ = NULL; }
                    ;

type                : TTYPE { $$ = ast_make_type($1); }
                    | type TMUL { $$ = ast_make_pointer($1); }
                    | type TLBRACKET TRBRACKET { $$ = ast_make_array($1, -1); }
                    | type TLBRACKET TINT TRBRACKET { $$ = ast_make_array($1, atoi($3)); }
                    | TCOUNTED type { $$ = ast_make_counted($2); }
                    | TWEAK type { $$ = ast_make_weak($2); }
                    | type TPOW { $$ = ast_make_counted(ast_make_pointer($1)); }
                    | TLPAREN typelist TCOLON type TRPAREN { $$ = ast_make_closure_type($2, $4); }
                    | TLPAREN TCOLON type TRPAREN { $$ = ast_make_closure_type(NULL, $3); }
                    | TLPAREN TCOLON TRPAREN { $$ = ast_make_closure_type(NULL, ast_make_type((char *)"void")); }
                    | TLPAREN typelist TCOLON TRPAREN { $$ = ast_make_closure_type($2, ast_make_type((char *)"void")); }
                    | TLPAREN TGEN TCOLON type TRPAREN { }
                    | TLBRACKET typelist TCOLON type TRBRACKET { $$ = ast_make_function_type($2, $4); }
                    | TLBRACKET TCOLON type TRBRACKET { $$ = ast_make_function_type(NULL, $3); }
                    | TLBRACKET typelist TCOLON TRBRACKET { $$ = ast_make_function_type($2, ast_make_type((char *)"void")); }
                    | TLBRACKET TCOLON TRBRACKET { $$ = ast_make_function_type(NULL, ast_make_type((char *)"void")); }
                    | compositetype
                    ;

compositetype       : TTYPE TOR TTYPE { $$ = ast_make_type($1); ast_make_composite($$, $3); }
                    | TTYPE TOR compositetype { $$ = ast_make_composite($3, $1); }
                    ;

structdecl          : TSTRUCT TTYPE TLBRACE structlist TRBRACE { $$ = $4; ast_struct_name($$, $2); }
                    | TSTRUCT TTYPE TLBRACE TSEMI structlist TRBRACE { $$ = $5; ast_struct_name($$, $2); }
                    ;

structlist          : structlist variabledecl TSEMI { $$ = $1; ast_struct_add($$, $2); }
                    | variabledecl TSEMI { $$ = ast_make_struct_decl(); ast_struct_add($$, $1); }
                    ;

interfacelist       : interfacelist funcsident TSEMI { $$ = $1; ast_class_method_add($$, $2); }
                    | interfacelist funcident TSEMI { $$ = $1; ast_class_method_add($$, $2); }
                    | funcsident TSEMI { $$ = ast_make_interface_decl(); ast_class_method_add($$, $1); }
                    | funcident TSEMI { $$ = ast_make_interface_decl(); ast_class_method_add($$, $1); }
                    ;

interfacedecl       : TINTERFACE TTYPE TLBRACE interfacelist TRBRACE { $$ = $4; ast_class_name($$, $2); }
                    | TINTERFACE TTYPE TLBRACE TSEMI interfacelist TRBRACE { $$ = $5; ast_class_name($$, $2); }
                    ;

interfacetypelist   : type { $$ = $1; }
                    | type TCOMMA interfacetypelist { $$ = $1; $$->next = $3; }
                    ;

classdecl           : TCLASS TTYPE TLBRACE classlist TRBRACE { $$ = $4; ast_class_name($$, $2); }
                    | TCLASS TTYPE TLBRACE TSEMI classlist TRBRACE { $$ = $5; ast_class_name($$, $2); }
                    | TCLASS TTYPE TLPAREN interfacetypelist TRPAREN TLBRACE classlist TRBRACE { $$ = $7; ast_class_name($$, $2); ast_class_add_interface($$, $4); }
                    | TCLASS TTYPE TLPAREN interfacetypelist TRPAREN TLBRACE TSEMI classlist TRBRACE { $$ = $8; ast_class_name($$, $2); ast_class_add_interface($$, $4); }
                    ;

classlist           : classlist variabledecl TSEMI { $$ = $1; ast_class_var_add($$, $2); }
                    | classlist funcdecl { $$ = $1; ast_class_method_add($$, $2); }
                    | classlist initdecl { $$ = $1; ast_class_set_init($$, $2); }
                    | classlist funcident TSEMI { $$ = $1; ast_class_method_add($$, $2); }
                    | classlist funcsident TSEMI { $$ = $1; ast_class_method_add($$, $2); }
                    | classlist viewdecl { $$ = $1; ast_class_method_add($$, $2); }
                    | variabledecl TSEMI { $$ = ast_make_class_decl(); ast_class_var_add($$, $1); }
                    | funcdecl { $$ = ast_make_class_decl(); ast_class_method_add($$, $1); }
                    | initdecl { $$ = ast_make_class_decl(); ast_class_set_init($$, $1); }
                    | funcident TSEMI { $$ = ast_make_class_decl(); ast_class_method_add($$, $1); }
                    | funcsident TSEMI { $$ = ast_make_class_decl(); ast_class_method_add($$, $1); }
                    ;

initdecl            : TIDENTIFIER TLPAREN TRPAREN block { $$ = ast_make_class_special_decl($1, $4, NULL); }
                    | TIDENTIFIER TLPAREN vardecllist TRPAREN block { $$ = ast_make_class_special_decl($1, $5, $3); }
                    | TIDENTIFIER TLPAREN TRPAREN TSEMI { $$ = ast_make_class_special_decl($1, NULL, NULL); }
                    | TIDENTIFIER TLPAREN vardecllist TRPAREN TSEMI { $$ = ast_make_class_special_decl($1, NULL, $3); }
                    ;

funcdecl            : funcident block { ((ASTFuncDecl *)$1)->body = $2; $$ = $1; };

gendecl             : genident block { ((ASTFuncDecl *)$1)->body = $2; $$ = $1; };

viewdecl            : viewident block { ((ASTFuncDecl *)$1)->body = $2; $$ = $1; }
                    ;

viewident           : TVIEW type { $$ = ast_make_func_decl($2, ett_unique_type_name(((ASTTypeDecl *)$2)->etype), NULL, NULL); }
                    ;

externdecl          : TEXTERN funcident { $$ = $2; }
                    | TEXTERN funcsident { $$ = $2; }
                    | TEXTERN classdecl { $$ = $2; ast_class_set_extern($$); }
                    | TEXTERN structdecl { $$ = $2; ast_struct_set_extern($$); }
                    ;

funcident           : TFUNC TIDENTIFIER TLPAREN TRPAREN TCOLON type { $$ = ast_make_func_decl($6, $2, NULL, NULL); }
                    | TFUNC TIDENTIFIER TLPAREN TRPAREN { $$ = ast_make_func_decl(ast_make_type((char *)"void"), $2, NULL, NULL); }
                    | TFUNC TIDENTIFIER TLPAREN vardecllist TRPAREN TCOLON type { $$ = ast_make_func_decl($7, $2, NULL, $4); }
                    | TFUNC TIDENTIFIER TLPAREN vardecllist TRPAREN { $$ = ast_make_func_decl(ast_make_type((char *)"void"), $2, NULL, $4); }
                    ;

genident            : TGEN TIDENTIFIER TLPAREN TRPAREN TCOLON type { $$ = ast_make_gen_decl($6, $2, NULL, NULL); }
                    | TGEN TIDENTIFIER TLPAREN vardecllist TRPAREN TCOLON type { $$ = ast_make_gen_decl($7, $2, NULL, $4); }
                    ;

clodecl             : TFUNC TLPAREN TRPAREN TCOLON type blockalt { $$ = ast_make_func_decl($5, NULL, $6, NULL); }
                    | TFUNC TLPAREN vardecllist TRPAREN TCOLON type blockalt { $$ = ast_make_func_decl($6, NULL, $7, $3); }
                    | TFUNC TLPAREN TRPAREN blockalt { $$ = ast_make_func_decl(ast_make_type((char *)"void"), NULL, $4, NULL); }
                    | TFUNC TLPAREN vardecllist TRPAREN blockalt { $$ = ast_make_func_decl(ast_make_type((char *)"void"), NULL, $5, $3); }
                    ;

funcsident          : TFUNC TIDENTIFIER TLPAREN typelist TRPAREN TCOLON type { $$ = ast_make_func_decl($7, $2, NULL, $4); }
                    | TFUNC TIDENTIFIER TLPAREN typelist TELLIPSES TRPAREN TCOLON type { $$ = ast_make_func_decl($8, $2, NULL, $4); ast_set_vararg($$); }
                    | TFUNC TIDENTIFIER TLPAREN typelist TRPAREN { $$ = ast_make_func_decl(ast_make_type((char *)"void"), $2, NULL, $4); }
                    ;

typelist            : type { $$ = ast_make_var_decl($1, NULL); }
                    | type TCOMMA typelist { AST *a = ast_make_var_decl($1, NULL); a->next = $3; $$ = a; };

vardecllist         : variabledecl { $$ = $1; }
                    | variabledecl TCOMMA vardecllist { $1->next = $3; $$ = $1; };

block               : TLBRACE statements TRBRACE TSEMI { $$ = $2; };

blockalt            : TLBRACE statements TRBRACE { $$ = $2; };

statements          : statement { if($1) $$ = $1; else $$ = ast_make(); }
                    | statement statements { if($1) $1->next = $2; $$ = $1 ? $1 : $2; };

statement           : expr TSEMI { $$ = $1; }
                    | TBREAK TSEMI { $$ = ast_make_unary(NULL, 'b'); }
                    | TCONTINUE TSEMI { $$ = ast_make_unary(NULL, 'c'); }
                    | TRETURN TSEMI { $$ = ast_make_unary(NULL, 'r'); }
                    | TRETURN expr TSEMI { $$ = ast_make_unary($2, 'r'); }
                    | TYIELD expr TSEMI { $$ = ast_make_unary($2, 'y'); }
                    | TPUTS expr TSEMI { $$ = ast_make_unary($2, 'p'); }
                    | TSEMI { $$ = NULL; }
                    | ifstatement { $$ = $1; }
                    | forstatement { $$ = $1; }
                    | TTOUCH expr TSEMI { $$ = ast_make_unary($2, 't'); }
                    /*| funcdecl { $$ = $1; }*/
                    ;

forstatement        : TFOR expr block { $$ = ast_make_loop(NULL, $2, NULL, $3); }
                    | TFOR expr TSEMI expr TSEMI expr block { $$ = ast_make_loop($2, $4, $6, $7); }
                    | TFOR expr TIN expr block { $$ = ast_make_loop($2, $4, NULL, $5); }
                    ;

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

variabledecl        : type TIDENTIFIER { $$ = ast_make_var_decl($1, $2); };
                    /*| TCOUNTED type TIDENTIFIER { $$ = ast_make_var_decl($2, $3); ast_set_counted($$); };*/

funccall            : ounexpr TLPAREN TRPAREN { $$ = ast_make_func_call($1, NULL); }
                    | ounexpr TLPAREN calllist TRPAREN { $$ = ast_make_func_call($1, $3); };

calllist            : expr { $$ = $1; }
                    | expr TCOMMA calllist { $1->next = $3; $$ = $1; };

expr                : binexpr { $$ = $1; }
                    | unexpr { $$ = $1; }
                    | singexpr { $$ = $1; }
                    | variabledecl { $$ = $1; }
                    | TVAR TIDENTIFIER { $$ = ast_make_auto_decl($2); }
                    | type TAT ounexpr { $$ = ast_make_cast($1, $3); }
                    | TNEW type { $$ = ast_make_allocater('n', $2, NULL); }
                    | TNEW type TLPAREN calllist TRPAREN { $$ = ast_make_allocater('n', $2, $4); }
                    | TNEW type TLPAREN TRPAREN { $$ = ast_make_allocater('n', $2, (void *)1); }
                    ;

binexpr             : expr TPLUS expr { $$ = ast_make_binary($1, $3, '+'); }
                    | expr TPLUSE expr { $$ = ast_make_binary($1, $3, 'P'); }
                    | expr TEQUALS expr { $$ = ast_make_binary($1, $3, '='); }
                    | expr TMINUS expr { $$ = ast_make_binary($1, $3, '-'); }
                    | expr TMINUSE expr { $$ = ast_make_binary($1, $3, 'M'); }
                    | expr TMUL expr { $$ = ast_make_binary($1, $3, '*'); }
                    | expr TMULE expr { $$ = ast_make_binary($1, $3, 'T'); }
                    | expr TDIV expr { $$ = ast_make_binary($1, $3, '/'); }
                    | expr TDIVE expr { $$ = ast_make_binary($1, $3, 'D'); }
                    | expr TEQ expr { $$ = ast_make_binary($1, $3, 'e'); }
                    | expr TGT expr { $$ = ast_make_binary($1, $3, 'g'); }
                    | expr TGTE expr { $$ = ast_make_binary($1, $3, 'G'); }
                    | expr TLT expr { $$ = ast_make_binary($1, $3, 'l'); }
                    | expr TLTE expr { $$ = ast_make_binary($1, $3, 'L'); }
                    | expr TNE expr { $$ = ast_make_binary($1, $3, 'n'); }
                    | expr TLOGAND expr { $$ = ast_make_binary($1, $3, '&'); }
                    | expr TLOGOR expr { $$ = ast_make_binary($1, $3, '|'); }
                    | expr TMOD expr { $$ = ast_make_binary($1, $3, '%'); }
                    ;

unexpr              : ounexpr TPOW { $$ = ast_make_unary($1, '*'); }
                    | TAMP ounexpr { $$ = ast_make_unary($2, '&'); }
                    | TNOT ounexpr { $$ = ast_make_unary($2, '!'); }
                    | TSIZEOF TLPAREN type TRPAREN { $$ = ast_make_unary($3, 's'); }
                    | TCOUNTOF ounexpr { $$ = ast_make_unary($2, 'c'); }
                    | TUNWRAP ounexpr { $$ = ast_make_unary($2, 'u'); }
                    | ounexpr TLBRACKET expr TRBRACKET { $$ = ast_make_binary($1, $3, '['); }
                    | ounexpr TDOT TIDENTIFIER { $$ = ast_make_struct_get($1, $3); }
                    | ounexpr TARROW TIDENTIFIER { $$ = ast_make_struct_get(ast_make_unary($1, '*'), $3); }
                    ; 

ounexpr             : singexpr { $$ = $1; }
                    | unexpr { $$ = $1; }
                    ;

singexpr            : TINT { $$ = ast_make_int32($1); }
                    | TDOUBLE { $$ = ast_make_double($1); }
                    | TCSTR { $$ = ast_make_cstr($1); }
                    | clodecl { $$ = $1; }
                    | TYES { $$ = ast_make_bool(1); }
                    | TNO { $$ = ast_make_bool(0); }
                    | TNIL { $$ = ast_make_nil(); }
                    | TIDENTIFIER { $$ = ast_make_identifier($1); }
                    | TLPAREN expr TRPAREN { $$ = $2; }
                    | funccall { $$ = $1; }
                    ;

%%
