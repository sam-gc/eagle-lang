#include <stdio.h>
#include "compiler/ast_compiler.h"
#include "compiler/ast.h"
#include "grammar/eagle.tab.h"
#include "utils.h"
#include "shipping.h"
#include "versioning.h"
#include "environment/imports.h"
#include "multibuffer.h"
#include "config.h"

#define YY_BUF_SIZE 32768
#define SEQU(a, b) strcmp((a), (b)) == 0

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

char *get_file_ext(const char *filename)
{
    int p = -1;
    char c;
    const char *iter = filename;
    int i;

    for(c = iter[0], i = 0; c; c = *(++iter), i++)
        if(c == '.') p = i;

    if(p < 0)
        return (char *)"";

    return (char *)(filename + p);
}

void fill_crate(ShippingCrate *crate, int argc, const char *argv[])
{
    crate->current_file = NULL;
    crate->source_files = arr_create(5);
    crate->object_files = arr_create(5);

    int skip = 0;
    int i;
    for(i = 1; i < argc; i++)
    {
        if(skip)
            continue;
        const char *arg = argv[i];
        if(SEQU(arg, "-o"))
            skip = 1;
        
        if(
            SEQU(arg, "-o") ||
            SEQU(arg, "-c") ||
            SEQU(arg, "-h") ||
            SEQU(arg, "-O0") || SEQU(arg, "-O1") || SEQU(arg, "-O2") || SEQU(arg, "-O3") ||
            SEQU(arg, "--llvm") ||
            SEQU(arg, "--no-rc") ||
            SEQU(arg, "--verbose") ||
            SEQU(arg, "--dump-code")) 
            continue;
        
        if(SEQU(get_file_ext(arg), ".o"))
            arr_append(&crate->object_files, (char *)arg);
        else
            arr_append(&crate->source_files, (char *)arg);
    }
}

void compile_generic(ShippingCrate *crate, int include_rc)
{
    YY_BUFFER_STATE yybuf = yy_create_buffer(NULL, YY_BUF_SIZE);
    yy_switch_to_buffer(yybuf);
    
    ty_prepare();
    first_pass();

    if(IN(global_args, "--dump-code"))
    {
        printf("Dump of:\n%s\n=================================================\n", crate->current_file);
        mb_print_all(ymultibuffer);
        printf("\n\n");

        return;
    }

    //mb_add_file(ymultibuffer, argv[1]);

    yyparse();

    mb_free(ymultibuffer);
    yy_delete_buffer(yybuf);

    LLVMModuleRef module = ac_compile(ast_root, include_rc);

    ty_teardown();

    shp_optimize(module);

    if(IN(global_args, "--llvm"))
        LLVMDumpModule(module);
    else
    {
        shp_produce_assembly(module);
        shp_produce_binary(crate);
    }

    LLVMDisposeModule(module);

    utl_free_registered();
    ast_free_nodes();
}

void compile_rc(ShippingCrate *crate)
{
    ymultibuffer = mb_alloc();
    mb_add_str(ymultibuffer, rc_code);

    crate->current_file = (char *)"__egl_rc_str.egl";

    if(IN(global_args, "--verbose"))
        printf("Compiling runtime...\n");

    compile_generic(crate, 0);
}

void compile_file(char *file, ShippingCrate *crate)
{
    ymultibuffer = imp_generate_imports(file);
    mb_add_file(ymultibuffer, file);
    crate->current_file = file;

    if(IN(global_args, "-h"))
    {
        char *text = mb_get_first_str(ymultibuffer);
        if(text)
            printf("%s\n", mb_get_first_str(ymultibuffer));
    }
    else
    {
        if(IN(global_args, "--verbose"))
            printf("Compiling file (%s)...\n", file);
        compile_generic(crate, !IN(global_args, "--no-rc"));
    }
}


int main(int argc, const char *argv[])
{
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

//    int token;
//    while ((token = yylex()) != 0)
//        printf("Token: %d (%s)\n", token, yytext);
//    return 0;
    
    ShippingCrate crate;
    fill_crate(&crate, argc, argv);

    for(i = 0; i < crate.source_files.count; i++)
        compile_file(crate.source_files.items[i], &crate);

    if(!IN(global_args, "-c") && !IN(global_args, "--llvm") && !IN(global_args, "-h") &&
       !IN(global_args, "--dump-code"))
    {
        if(!IN(global_args, "--no-rc"))
            compile_rc(&crate);
        shp_produce_executable(&crate);
    }
    
    return 0;
}
