#include <stdio.h>
#include "compiler/ast_compiler.h"
#include "compiler/ast.h"
#include "grammar/eagle.tab.h"
#include "utils.h"
#include "shipping.h"
#include "versioning.h"

extern char *yytext;

extern FILE *yyin;
extern int yyparse();
extern int yylex();
extern int yylineno;

extern AST *ast_root;

hashtable global_args;

void first_pass()
{
    int token;
    int saveNextStruct = 0;
    int saveNextClass = 0;
    int saveNextInterface = 0;
    while((token = yylex()) != 0)
    {
        if(saveNextStruct)
        {
            ty_add_name(yytext);
            saveNextStruct = 0;
        }
        if(saveNextClass)
        {
            ty_register_class(yytext);
            saveNextClass = 0;
        }
        if(saveNextInterface)
        {
            ty_register_interface(yytext);
            saveNextInterface = 0;
        }
        saveNextStruct = (token == TSTRUCT || token == TCLASS || token == TINTERFACE);
        saveNextClass = token == TCLASS;
        saveNextInterface = token == TINTERFACE;
    }

    rewind(yyin);
    yylineno = 0;
}

int main(int argc, const char *argv[])
{
    etTargetData = LLVMCreateTargetData("");
    global_args = hst_create();

    if(argc < 2)
    {
        printf("Usage: %s <file> [args]\n", argv[0]);
        return 0;
    }

    int i;
    for(i = 0; i < argc - 1; i++)
        hst_put(&global_args, (char *)argv[i], (void *)argv[i + 1], NULL, NULL);
    hst_put(&global_args, (char *)argv[i], (void *)1, NULL, NULL);

    if(IN(global_args, "--version") || IN(global_args, "-v"))
    {
        print_version_info();
        return 0;
    }

    yyin = fopen(argv[1], "r");
    if(!yyin)
    {
        printf("Error: could not find file.\n");
        return 0;
    }

    ty_prepare();

    first_pass();

    yyparse();

    LLVMModuleRef module = ac_compile(ast_root);

    ty_teardown();

    shp_optimize(module);

    if(IN(global_args, "--llvm"))
        LLVMDumpModule(module);
    else
    {
        shp_produce_assembly(module);
        shp_produce_binary();
    }

    LLVMDisposeModule(module);

    LLVMDisposeTargetData(etTargetData);

    utl_free_registered();
    ast_free_nodes();
    
//    int token;
//    while ((token = yylex()) != 0)
//        printf("Token: %d (%s)\n", token, yytext);
//    return 0;
    
    return 0;
}
