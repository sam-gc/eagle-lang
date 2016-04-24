/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "utils.h"
#include "mempool.h"
#include "compiler/ast_compiler.h"
#include <string.h>

char *utl_gen_escaped_string(char *inp, int lineno)
{
    char *n = malloc(strlen(inp) + 1);
    utl_register_memory(n);
    unsigned i, j;
    for(i = j = 0; i < strlen(inp); i++, j++)
    {
        if(inp[i] != '\\')
        {
            n[j] = inp[i];
            continue;
        }

        i++;
        char gen = 0;
        switch(inp[i])
        {
            case 0:
                die(lineno, "Unexpected end of escaped string: \"%s\"", inp);
            case 't':
                gen = '\t';
                break;
            case 'n':
                gen = '\n';
                break;
            case 'a':
                gen = '\a';
                break;
            case 'b':
                gen = '\b';
                break;
            case 'f':
                gen = '\f';
                break;
            case 'r':
                gen = '\r';
                break;
            case 'v':
                gen = '\v';
                break;
            case '\\':
                gen = '\\';
                break;
            case '\'':
                gen = '\'';
                break;
            default:
                die(lineno, "Unknown escape character in string: \"%s\"", inp);
        }

        n[j] = gen;
    }

    n[j] = 0;

    return n;
}

static Mempool utl_mempool;
static int init_mempool = 0;

void utl_register_memory(void *m)
{
    if(!init_mempool)
    {
        init_mempool = 1;
        utl_mempool = pool_create();
    }

    pool_add(&utl_mempool, m);
}

void utl_free_registered()
{
    pool_drain(&utl_mempool);
}

static LLVMContextRef the_context = NULL;
void utl_set_current_context(LLVMContextRef ctx)
{
    the_context = ctx;
}

LLVMContextRef utl_get_current_context()
{
    return the_context;
}
