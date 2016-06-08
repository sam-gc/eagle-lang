/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "arguments.h"
#include "termargs.h"
#include "config.h"
#include "versioning.h"

#define SEQU(a, b) strcmp((a), (b)) == 0

extern Hashtable global_args;
extern int proper_formatting;

static TermArgs *targs;
static const char *progname;

static char *get_file_ext(const char *filename)
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

static void rule_code(char *arg, char *next, int *skip, void *data)
{
    ShippingCrate *crate = data;

    if(!next)
        die(-1, msgerr_arg_code_missing);
    arr_append(&crate->extra_code, (void *)next);
    *skip = 1;
}

static void rule_version(char *arg, char *next, int *skip, void *data)
{
    print_version_info();
    args_teardown();
    exit(0);
}

static void rule_help(char *arg, char *next, int *skip, void *data)
{ 
    printf("Usage: %s [options] code-file(s)\nOptions:\n", progname);
    ta_help(targs);
    args_teardown();
    exit(0);
}

static void rule_threads(char *arg, char *next, int *skip, void *data)
{
    if(!next)
        die(-1, msgerr_arg_threads_missing, arg);
    *skip = 1;
    ShippingCrate *crate = data;
    crate->threadct = atoi(next);

    if(!crate->threadct)
        warn(-1, msgwarn_invalid_thread_count);
}

static void rule_verbose(char *arg, char *next, int *skip, void *data)
{
    ShippingCrate *crate = data;
    crate->verbose = 1;
}

static void rule_skip(char *arg, char *next, int *skip, void *data)
{
    if(!next)
        die(-1, msgerr_arg_skip_missing, arg);
    *skip = 1;
}

static void rule_pf(char *arg, char *next, int *skip, void *data)
{
    proper_formatting = 1;
}

static void rule_ignore(char *arg, char *next, int *skip, void *data)
{
    // Pass
}

static void rule_seive(char *arg, char *next, int *skip, void *data)
{
    ShippingCrate *crate = data;

    if(strlen(arg) > 2 && arg[0] == '-' && arg[1] == 'l')
    {
        arr_append(&crate->libs, (char *)arg);
        return;
    }
    else if(strlen(arg) > 2 && arg[0] == '-' && arg[1] == 'L')
    {
        arr_append(&crate->lib_paths, (char *)arg);
        return;
    }

    if(access(arg, R_OK) < 0)
    {
        warn(-1, msgwarn_ignoring_unknown_param, arg);
        return;
    }

    if(SEQU(get_file_ext(arg), ".o"))
        arr_append(&crate->object_files, (char *)arg);
    else
        arr_append(&crate->source_files, (char *)arg);
}

void args_setup(ShippingCrate *crate)
{
    targs = ta_new(&global_args, crate);
    ta_rule(targs, "--help", "--help", &rule_help, "Display command information");
    ta_rule(targs, "--version", "--version", &rule_version, "Display version number and copyright information");
    ta_rule(targs, "--llvm", "--llvm", &rule_ignore, "Dump LLVM IR code to stderr");
    ta_rule(targs, "--no-rc", "--no-rc", &rule_ignore, "Do not include reference counting symbols in module");
    ta_rule(targs, "--verbose", "--verbose", &rule_verbose, "Display verbose output during compilation");
    ta_rule(targs, "--code", "--code <eagle code>", &rule_code, "Provide extra code to compile");
    ta_rule(targs, "--threads", "--threads <count>", &rule_threads, "Optimize and compile on <count> threads (default 4)");
    ta_rule(targs, "--dump-code", "--dump-code", &rule_ignore, "Dump the pre-processed code from imports");
    ta_rule(targs, "--pf", "--pf", &rule_pf, "Require proper formatting for code (strict mode)");
    ta_rule(targs, "-o", "-o <filename>", &rule_skip, "Output executable name");
    ta_rule(targs, "-c", "-c", &rule_ignore, "Output object file");
    ta_rule(targs, "-S", "-S", &rule_ignore, "Output assembly file");

    ta_rule(targs, "-O0", "-O<0-3>", &rule_ignore, "Specify optimization level (default 2)");
    ta_rule(targs, "-O1", NULL, &rule_ignore, NULL);
    ta_rule(targs, "-O2", NULL, &rule_ignore, NULL);
    ta_rule(targs, "-O3", NULL, &rule_ignore, NULL);
    ta_rule(targs, "*", NULL, &rule_seive, NULL);

    ta_extra(targs, "-l<libname>", "Link with library");
    ta_extra(targs, "-L<libpath>", "Add linker search path");
}

void args_run(const char *argv[])
{
    progname = argv[0];
    ta_run(targs, argv + 1);
}

void args_teardown()
{
    ta_free(targs);
}

