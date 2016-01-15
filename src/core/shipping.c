#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <libgen.h>
#include <unistd.h>
#include "cpp/cpp.h"
#include "shipping.h"
#include "hashtable.h"
#include "config.h"
#include "stringbuilder.h"
#include "threading.h"
#include "mempool.h"

static mempool unlink_pool;
extern hashtable global_args;
typedef LLVMPassManagerBuilderRef LPMB;

void shp_setup()
{
    unlink_pool = pool_create();
    unlink_pool.free_func = (void (*)(void *))unlink;
}

void shp_teardown()
{
    pool_drain(&unlink_pool);
}

void strip_ext(char *base)
{
    int i;
    int len = strlen(base);
    for(i = len - 1; i >= 0; i--)
        if(base[i] == '.')
        {
            base[i] = 0;
            break;
        }
}

char *shp_temp_object_file(char *filename)
{
    char *dup = strdup(filename);
    char *base = basename(dup);
    strip_ext(base);

    char *name = malloc(100);
    sprintf(name, "/tmp/egl%d_%s.o", thr_request_number(), base);
    free(dup);

    pool_add(&unlink_pool, name);
    return name;
}

char *shp_temp_assembly_file(char *filename)
{
    char *dup = strdup(filename);
    char *base = basename(dup);
    strip_ext(base);

    char *name = malloc(100);
    sprintf(name, "/tmp/egl%d_%s.s", thr_request_number(), base);
    free(dup);

    pool_add(&unlink_pool, name);
    return name;
}

void shp_optimize(LLVMModuleRef module)
{
    LPMB passBuilder = LLVMPassManagerBuilderCreate();
    LLVMPassManagerRef pm = LLVMCreatePassManager();

    unsigned opt = 2;

    if(IN(global_args, "-O0"))
        opt = 0;
    else if(IN(global_args, "-O1"))
        opt = 1;
    else if(IN(global_args, "-O3"))
        opt = 3;

    LLVMPassManagerBuilderSetOptLevel(passBuilder, opt);
    LLVMPassManagerBuilderPopulateModulePassManager(passBuilder, pm);

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();

    LLVMTargetDataRef td = LLVMCreateTargetData("");
    LLVMAddTargetData(td, pm);

    LLVMRunPassManager(pm, module);

    LLVMPassManagerBuilderDispose(passBuilder);
}

char *shp_switch_file_ext(char *orig, const char *n)
{
    char *out = malloc(strlen(orig) + strlen(n) + 1);
    memcpy(out, orig, strlen(orig) + 1);
    int p = -1;
    char c;
    int i;
    for(c = orig[0], i = 0; c; c = *(++orig), i++)
        if(c == '.')
            p = i;

    if(p < 0)
    {
        p = strlen(out);
        out[p] = '.';
    }

    memcpy(out + p + 1, n, strlen(n) + 1);

    return out;
}

void shp_produce_assembly(LLVMModuleRef module, ShippingCrate *crate)
{
    // EGLGenerateAssembly(module, "/tmp/egl_out.s");

    char *ofn = NULL;

    if(IN(global_args, "-S"))
        ofn = shp_switch_file_ext(crate->current_file, "s");
    else
        ofn = shp_temp_assembly_file(crate->current_file);

    LLVMTargetRef targ;
    LLVMGetTargetFromTriple(LLVMGetDefaultTargetTriple(), &targ, NULL);
    LLVMTargetMachineRef tm = LLVMCreateTargetMachine(targ, LLVMGetDefaultTargetTriple(), "", "", LLVMCodeGenLevelNone, LLVMRelocDefault, LLVMCodeModelDefault);
    LLVMTargetMachineEmitToFile(tm, module, ofn, LLVMAssemblyFile, NULL);

    crate->current_temp_assembly = ofn;

    LLVMDisposeTargetMachine(tm);
}

void shp_produce_binary(ShippingCrate *crate)
{
    char *outfile = shp_temp_object_file(crate->current_file);

    char command[500 + strlen(outfile)];

    if(IN(global_args, "-c"))
    {
        free(outfile);
        outfile = shp_switch_file_ext(crate->current_file, "o");
    }

    sprintf(command, CC " -c %s -o %s -g -O0", crate->current_temp_assembly, outfile); //objectfile, rcfile, outfile);
    
    system(command);

    arr_append(&crate->object_files, outfile);
}

void shp_produce_executable(ShippingCrate *crate)
{
    char *outfile = (char *)"a.out";

    if(IN(global_args, "-o"))
        outfile = hst_get(&global_args, (char *)"-o", NULL, NULL);

    Strbuilder sbd;
    sb_init(&sbd);
    int i;
    for(i = 0; i < crate->object_files.count; i++)
    {
        sb_append(&sbd, crate->object_files.items[i]);
        sb_append(&sbd, " ");
    }

    char command[500 + strlen(outfile) + strlen(sbd.buffer)];

    //const char *rcfile = IN(global_args, "--no-rc") ? "" : "rc.o";
    sprintf(command, CC " %s -o %s -lm", sbd.buffer, outfile);
    free(sbd.buffer); 

    system(command);
}

