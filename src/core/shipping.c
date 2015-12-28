#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "cpp/cpp.h"
#include "shipping.h"
#include "hashtable.h"
#include "config.h"
#include "stringbuilder.h"

extern hashtable global_args;
typedef LLVMPassManagerBuilderRef LPMB;

char *shp_temp_object_file()
{
    static int counter = 0;
    char *name = malloc(100);
    sprintf(name, "/tmp/egl_%d.o", counter++);

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

void shp_produce_assembly(LLVMModuleRef module)
{
    EGLGenerateAssembly(module);
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

void shp_produce_binary(ShippingCrate *crate)
{
    char *outfile = shp_temp_object_file();

    /*
    if(IN(global_args, "-o"))
        outfile = hst_get(&global_args, (char *)"-o", NULL, NULL);
        */

    char command[500 + strlen(outfile)];

    /*
    const char *objectfile = "";
    const char *rcfile = "rc.o";
    */
    if(IN(global_args, "-c"))
    {
        free(outfile);
        outfile = shp_switch_file_ext(crate->current_file, "o");
        /*
        objectfile = "-c";
        rcfile = "";
        */
    }

    sprintf(command, CC " -c /tmp/egl_out.s -o %s -g", outfile); //objectfile, rcfile, outfile);
    
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

    const char *rcfile = IN(global_args, "--no-rc") ? "" : "rc.o";
    sprintf(command, CC " %s %s -o %s -lm", rcfile, sbd.buffer, outfile);
    free(sbd.buffer); 

    system(command);
}

