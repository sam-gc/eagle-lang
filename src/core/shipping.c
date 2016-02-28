/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

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

extern hashtable global_args;
typedef LLVMPassManagerBuilderRef LPMB;

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

    thr_populate_pass_manager(passBuilder, pm);

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

void shp_produce_assembly(LLVMModuleRef module, char *filename, char **outname)
{
    // EGLGenerateAssembly(module, "/tmp/egl_out.s");

    char *ofn = NULL;

    if(IN(global_args, "-S"))
        ofn = shp_switch_file_ext(filename, "s");
    else
        ofn = thr_temp_assembly_file(filename);

    LLVMTargetRef targ;
    LLVMGetTargetFromTriple(LLVMGetDefaultTargetTriple(), &targ, NULL);
    LLVMTargetMachineRef tm = LLVMCreateTargetMachine(targ, LLVMGetDefaultTargetTriple(), "", "", LLVMCodeGenLevelNone, LLVMRelocDefault, LLVMCodeModelDefault);
    LLVMTargetMachineEmitToFile(tm, module, ofn, LLVMAssemblyFile, NULL);

    *outname = ofn;

    LLVMDisposeTargetMachine(tm);
}

void shp_produce_binary(char *filename, char *assemblyname, char **outname)
{
    if(IN(global_args, "-S"))
        return;

    char *outfile = thr_temp_object_file(filename);

    char command[500 + strlen(outfile)];

    if(IN(global_args, "-c"))
    {
        free(outfile);
        outfile = shp_switch_file_ext(filename, "o");
    }

    sprintf(command, CC " -c %s -o %s -g -O0", assemblyname, outfile); //objectfile, rcfile, outfile);

    system(command);

    *outname = outfile;
    // arr_append(&crate->object_files, outfile);
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

    Strbuilder libb;
    sb_init(&libb);
    for(i = 0; i < crate->libs.count; i++)
    {
        sb_append(&libb, crate->libs.items[i]);
        sb_append(&libb, " ");
    }

    char command[500 + strlen(outfile) + strlen(sbd.buffer) + strlen(libb.buffer)];

    //const char *rcfile = IN(global_args, "--no-rc") ? "" : "rc.o";
    sprintf(command, CC " %s -o %s %s", sbd.buffer, outfile, libb.buffer);
    free(sbd.buffer);
    free(libb.buffer);

    system(command);
}
