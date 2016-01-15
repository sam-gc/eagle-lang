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
#include "cpp/cpp.h"
#include "threading.h"

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
    crate->extra_code = arr_create(2);

    int skip = 0;
    int i;
    for(i = 1; i < argc; i++)
    {
        if(skip)
        {
            skip = 0;
            continue;
        }
        const char *arg = argv[i];

        if(SEQU(arg, "--code"))
        {
            if(i == argc - 1)
                die(-1, "--code switch specified but no extra code defined.");
            arr_append(&crate->extra_code, (void *)argv[i + 1]);
        }

        if(SEQU(arg, "-o") || SEQU(arg, "--code"))
            skip = 1;
        
        if(
            SEQU(arg, "-o") ||
            SEQU(arg, "-c") ||
            SEQU(arg, "-S") ||
            SEQU(arg, "-h") ||
            SEQU(arg, "-O0") || SEQU(arg, "-O1") || SEQU(arg, "-O2") || SEQU(arg, "-O3") ||
            SEQU(arg, "--llvm") ||
            SEQU(arg, "--no-rc") ||
            SEQU(arg, "--verbose") ||
            SEQU(arg, "--code") ||
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
        shp_produce_assembly(module, crate);
        shp_produce_binary(crate);
    }

    LLVMDisposeModule(module);

    utl_free_registered();
    ast_free_nodes();
}

char *make_argcode_name()
{
    static int argcode_counter = 0;
    char *text = malloc(100);
    sprintf(text, "__egl_argcode_%d.egl", argcode_counter++);
    return text;
}

void compile_string(char *str, ShippingCrate *crate)
{
    ymultibuffer = mb_alloc();
    mb_add_str(ymultibuffer, str);

    crate->current_file = make_argcode_name();

    if(IN(global_args, "--verbose"))
        printf("Compiling extra code {{\n%s\n}}\n", str);

    compile_generic(crate, !IN(global_args, "--no-rc"));

    free(crate->current_file);
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

    thr_init();
    shp_setup();

    ShippingCrate crate;
    fill_crate(&crate, argc, argv);

    for(i = 0; i < crate.source_files.count; i++)
    {
        utl_set_current_context(LLVMContextCreate());
        compile_file(crate.source_files.items[i], &crate);
        LLVMContextDispose(utl_get_current_context());
    }

    for(i = 0; i < crate.extra_code.count; i++)
    {
        utl_set_current_context(LLVMContextCreate());
        compile_string(crate.extra_code.items[i], &crate);
        LLVMContextDispose(utl_get_current_context());
    }

    if(!IN(global_args, "-c") && !IN(global_args, "--llvm") && !IN(global_args, "-h") &&
       !IN(global_args, "--dump-code") && !IN(global_args, "-S"))
    {
        if(!IN(global_args, "--no-rc"))
        {
            utl_set_current_context(LLVMContextCreate());
            compile_rc(&crate);
            LLVMContextDispose(utl_get_current_context());
        }
        shp_produce_executable(&crate);
    }

    shp_teardown();
    
    return 0;
}
