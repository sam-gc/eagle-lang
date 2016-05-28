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
#include <sys/wait.h>
#include "cpp/cpp.h"
#include "shipping.h"
#include "hashtable.h"
#include "config.h"
#include "stringbuilder.h"
#include "threading.h"
#include "mempool.h"

extern Hashtable global_args;
typedef LLVMPassManagerBuilderRef LPMB;

static void shp_spawn_process(const char *process, const char *args[]);

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
    char *ofn = NULL;

    if(IN(global_args, "-S"))
        ofn = shp_switch_file_ext(filename, "s");
    else
        ofn = thr_temp_assembly_file(filename);

    LLVMTargetRef targ;
    LLVMGetTargetFromTriple(LLVMGetDefaultTargetTriple(), &targ, NULL);
    LLVMTargetMachineRef tm =
        LLVMCreateTargetMachine(targ, LLVMGetDefaultTargetTriple(),
                                "", "", LLVMCodeGenLevelNone,
                                LLVMRelocDefault, LLVMCodeModelDefault);

    LLVMTargetMachineEmitToFile(tm, module, ofn, LLVMAssemblyFile, NULL);

    *outname = ofn;

    LLVMDisposeTargetMachine(tm);
}

void shp_produce_binary(char *filename, char *assemblyname, char **outname)
{
    if(IN(global_args, "-S"))
        return;

    char *outfile = thr_temp_object_file(filename);

    if(IN(global_args, "-c"))
    {
        free(outfile);
        outfile = shp_switch_file_ext(filename, "o");
    }

    const char *command[] = {
        SystemCC, "-c", assemblyname, "-o", outfile, "-g", "-O0", NULL
    };

    shp_spawn_process(SystemCC, command);

    *outname = outfile;
}

void shp_produce_executable(ShippingCrate *crate)
{
    char *outfile = (char *)"a.out";

    if(IN(global_args, "-o"))
        outfile = hst_get(&global_args, (char *)"-o", NULL, NULL);

    int arg_count = 4 + crate->object_files.count + crate->libs.count +
        crate->lib_paths.count;
    const char *args[arg_count];
    args[0] = SystemCC;
    args[1] = "-o";
    args[2] = outfile;

    int offset = 3;

    for(int i = 0; i < crate->object_files.count; i++)
        args[i + offset] = crate->object_files.items[i];

    offset += crate->object_files.count;

    for(int i = 0; i < crate->libs.count; i++)
        args[i + offset] = crate->libs.items[i];

    offset += crate->libs.count;

    for(int i = 0; i < crate->lib_paths.count; i++)
        args[i + offset] = crate->lib_paths.items[i];

    args[arg_count - 1] = NULL;

    shp_spawn_process(SystemCC, args);
}

static void shp_spawn_process(const char *process, const char *args[])
{
    pid_t pid = fork();
    if(!pid) // We are the child
    {
        execvp(process, (char *const*)args);
        die(-1, "Internal compiler error: execvp() should not return!");
    }
    else if(pid < 0)
    {
        die(-1, "Internal compiler error: failed to fork process");
    }

    int status;
    waitpid(pid, &status, 0);
}

