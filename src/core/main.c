#include <stdio.h>
#include "compiler/ast_compiler.h"
#include "compiler/ast.h"
#include "grammar/eagle.tab.h"
#include "utils.h"
#include "shipping.h"
#include "versioning.h"
#include "environment/imports.h"
#include "multibuffer.h"

#define YY_BUF_SIZE 32768

extern char *yytext;

extern FILE *yyin;
extern int yyparse();
extern int yylex();
extern int yylineno;

extern AST *ast_root;

hashtable global_args;
multibuffer *ymultibuffer = NULL;

typedef void * YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_create_buffer(FILE*, size_t);
extern void yy_switch_to_buffer(YY_BUFFER_STATE buf);
extern void yy_delete_buffer(YY_BUFFER_STATE buf);

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

    //rewind(yyin);
    mb_rewind(ymultibuffer);
    //mb_free(ymultibuffer);
    //ymultibuffer = nbuffer;
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

    /*
    yyin = fopen(argv[1], "r");
    if(!yyin)
    {
        printf("Error: could not find file.\n");
        return 0;
    }
    */

    ymultibuffer = imp_generate_imports(argv[1]);
    mb_add_file(ymultibuffer, argv[1]);

    YY_BUFFER_STATE yybuf = yy_create_buffer(NULL, YY_BUF_SIZE);
    yy_switch_to_buffer(yybuf);
    
    ty_prepare();
    first_pass();

    //mb_add_file(ymultibuffer, argv[1]);

    if(IN(global_args, "-h"))
    {
        imp_scan_file(argv[1]);
    }

    yyparse();

    mb_free(ymultibuffer);
    yy_delete_buffer(yybuf);

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
