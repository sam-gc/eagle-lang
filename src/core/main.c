/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <sys/time.h>
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
#include "arguments.h"
#include "colors.h"

#define YY_BUF_SIZE 32768
#define SEQU(a, b) strcmp((a), (b)) == 0

extern char *yytext;

extern FILE *yyin;
extern int yyparse();
extern void yyset_start_token(int);
extern int yylex();
extern int yylineno;

extern AST *ast_root;

Hashtable global_args;
Multibuffer *ymultibuffer = NULL;

typedef void * YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_create_buffer(FILE*, size_t);
extern void yy_switch_to_buffer(YY_BUFFER_STATE buf);
extern void yy_delete_buffer(YY_BUFFER_STATE buf);

char *current_file_name = NULL;

int proper_formatting;

static void register_typedef()
{
    char *prev = NULL;
    int token;
    int count = 0;
    int same = 0;
    while((token = yylex()) != TSEMI)
    {
        count++;

        same = prev && strcmp(prev, yytext) == 0;
        if(prev)
            free(prev);
        prev = strdup(yytext);
    }

    if(count == 2 && same)
        return;

    ty_add_name(prev);
    ty_register_typedef(prev);
    free(prev);
}

static void first_pass()
{
    int token;
    int saveNextStruct = 0;
    int saveNextClass = 0;
    int saveNextInterface = 0;
    int saveNextEnum = 0;
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
        if(saveNextEnum)
        {
            ty_register_enum(yytext);
            saveNextEnum = 0;
        }

        saveNextStruct = (token == TSTRUCT || token == TCLASS || token == TINTERFACE || token == TENUM);
        saveNextClass = token == TCLASS;
        saveNextInterface = token == TINTERFACE;
        saveNextEnum = token == TENUM;

        if(token == TTYPEDEF)
            register_typedef();
    }

    //rewind(yyin);
    mb_rewind(ymultibuffer);
    //mb_free(ymultibuffer);
    //yMultibuffer = nbuffer;
    yylineno = 0;
}

static void init_crate(ShippingCrate *crate)
{
    *crate = (ShippingCrate) {
        .source_files = arr_create(5),
        .object_files = arr_create(5),
        .extra_code   = arr_create(2),
        .work         = arr_create(10),
        .libs         = arr_create(5),
        .lib_paths    = arr_create(2),

        .verbose      = 0,
        .threadct     = 0 // Let the compiler choose later
    };
}

static LLVMModuleRef compile_generic(ShippingCrate *crate, int include_rc, char *file)
{
    YY_BUFFER_STATE yybuf = yy_create_buffer(NULL, YY_BUF_SIZE);
    yy_switch_to_buffer(yybuf);

    ty_prepare();
    if(IN(global_args, "--dump-code"))
    {
        printf("Dump of:\n%s\n=================================================\n", file);
        mb_print_all(ymultibuffer);
        printf("\n\n");

        return NULL;
    }

    first_pass();
    current_file_name = file;

    //mb_add_file(ymultibuffer, argv[1]);

    yyset_start_token(T_PARSE_PROGRAM);
    yyparse();

    mb_free(ymultibuffer);
    yy_delete_buffer(yybuf);

    LLVMModuleRef module = ac_compile(ast_root, include_rc);

    ty_teardown();

    if(IN(global_args, "--llvm"))
        LLVMDumpModule(module);

    utl_free_registered();
    ast_free_nodes();

    return module;
}

static char *make_argcode_name()
{
    static int argcode_counter = 0;
    char *text = malloc(100);
    sprintf(text, "__egl_argcode_%d.egl", argcode_counter++);
    return text;
}

static void compile_string(char *str, ShippingCrate *crate)
{
    char *file = make_argcode_name();
    ymultibuffer = mb_alloc();
    mb_add_str(ymultibuffer, str);

    // crate->current_file = make_argcode_name();
    utl_set_current_context(LLVMContextCreate());

    if(crate->verbose)
        printf(BLUE "Compiling extra code" DEFAULT ": {{\n%s\n}}\n", str);

    LLVMModuleRef module = compile_generic(crate, !IN(global_args, "--no-rc"), file);

    arr_append(&crate->work, thr_create_bundle(module, utl_get_current_context(), file));

    // free(crate->current_file);
}

static void compile_rc(ShippingCrate *crate)
{
    ymultibuffer = mb_alloc();
    mb_add_str(ymultibuffer, rc_code);

    // crate->current_file = (char *)"__egl_rc_str.egl";
    utl_set_current_context(LLVMContextCreate());

    if(crate->verbose)
        printf(BLUE "Compiling runtime\n" DEFAULT);

    LLVMModuleRef module = compile_generic(crate, 0, (char *)"__egl_rc_str.egl");
    arr_append(&crate->work, thr_create_bundle(module, utl_get_current_context(), (char *)"__egl_rc_str.egl"));
}

static void compile_file(char *file, ShippingCrate *crate)
{
    ymultibuffer = imp_generate_imports(file);
    mb_add_file(ymultibuffer, file);
    // crate->current_file = file;

    utl_set_current_context(LLVMContextCreate());

    if(crate->verbose)
        printf(BLUE "Compiling file" DEFAULT " -- %s\n", file);

    LLVMModuleRef module = compile_generic(crate, !IN(global_args, "--no-rc"), file);
    arr_append(&crate->work, thr_create_bundle(module, utl_get_current_context(), file));
}

static long getms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void pretty_print_time(long ms, char *buf)
{
    if(ms > 1500)
    {
        double frac = ms / 1000.0;
        sprintf(buf, "%lf s", frac);
        return;
    }

    sprintf(buf, "%ld ms", ms);
}

int main(int argc, const char *argv[])
{
    global_args = hst_create();

    ShippingCrate crate;
    init_crate(&crate);

    current_file_name = (char *)"program arguments";

    args_setup(&crate);

    if(argc < 2)
    {
        printf("Usage: %s [options] code-file(s)\n", argv[0]);
        return 0;
    }

    args_run(argv);

    if(!crate.source_files.count && !crate.object_files.count && !crate.extra_code.count)
    {
        die(-1, msgerr_no_code_specified);
    }

    if(crate.verbose)
        printf(BOLD "Starting build phase\n" DEFAULT);

    long start_time = getms();

    thr_init();
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();

    int i;
    for(i = 0; i < crate.source_files.count; i++)
        compile_file(crate.source_files.items[i], &crate);

    for(i = 0; i < crate.extra_code.count; i++)
        compile_string(crate.extra_code.items[i], &crate);

    if(!IN(global_args, "-c") && !IN(global_args, "--llvm") && !IN(global_args, "-h") &&
       !IN(global_args, "--dump-code") && !IN(global_args, "-S") && !IN(global_args, "--no-rc"))
        compile_rc(&crate);

    if(!IN(global_args, "--dump-code") && !IN(global_args, "--llvm"))
        thr_produce_machine_code(&crate);

    if(!IN(global_args, "-c") && !IN(global_args, "--llvm") && !IN(global_args, "-h") &&
       !IN(global_args, "--dump-code") && !IN(global_args, "-S"))
    {
        if(crate.verbose)
            printf(BOLD "Linking\n" DEFAULT);
        shp_produce_executable(&crate);
    }

    thr_teardown();

    long end_time = getms();

    if(crate.verbose)
    {
        char time[50];
        pretty_print_time(end_time - start_time, time);
        printf(BOLD "Build complete" DEFAULT " (finished in %s)\n", time);
    }

    return 0;
}

