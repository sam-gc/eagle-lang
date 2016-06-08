/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "compiler/ast.h"
#include "eagle.tab.h"
#include "core/hashtable.h"

#define LOOKBACK 3
#define PLACE_HOLDER (void *)(1)

extern int yylex();
extern char *yytext;

static int in_type_context;
static Hashtable type_names;
static int initialize_type_names = 1;

static void pipe_shift(int *arr, int tok, int ct)
{
    for(int i = 0; i < ct - 1; i++)
        arr[i] = arr[i + 1];

    arr[ct - 1] = tok;
}

static int pipe_matches(int *arr1, int *arr2, int ct)
{
    for(int i = 0; i < ct; i++)
        if(arr1[i] != arr2[i])
            return 0;
    return 1;
}

static void pipe_prepare_type_names()
{
    if(initialize_type_names)
    {
        type_names = hst_create();
        type_names.duplicate_keys = 1;
        initialize_type_names = 0;
        return;
    }

    hst_free(&type_names);
    type_names = hst_create();
    type_names.duplicate_keys = 1;
}

static void pipe_read_typenames()
{
    int tok;

    while((tok = yylex()) != TGT)
    {
        if(tok == TIDENTIFIER)
        {
            hst_put(&type_names, yytext, PLACE_HOLDER, NULL, NULL);
        }
    }
}

int pipe_lex()
{
    static int previous_tokens[LOOKBACK];
    static int target_tokens[] = {
        TFUNC,
        TIDENTIFIER,
        TLT
    };

    static int needs_generic_tok;

    int tok = yylex();
    pipe_shift(previous_tokens, tok, LOOKBACK);

    if(pipe_matches(previous_tokens, target_tokens, LOOKBACK))
    {
        pipe_prepare_type_names();
        pipe_read_typenames();
        in_type_context = 1;
        needs_generic_tok = 1;
        return yylex();
    }
    else if(tok == TMACRO)
    {

    }
    else if(tok == TLBRACE && needs_generic_tok > 0)
    {
        needs_generic_tok = -1;
        return TGENERIC;
    }
    else if(needs_generic_tok < 0)
    {
        needs_generic_tok = 0;
        return TLBRACE;
    }

    return tok;
}

int pipe_is_type(char *txt)
{
    if(!in_type_context)
        return 0;

    return (int)(uintptr_t)hst_get(&type_names, txt, NULL, NULL);
}

void pipe_reset_context()
{
    in_type_context = 0;
}

