/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "termargs.h"

typedef struct
{
    const char *fmt;
    const char *help;
    ta_rule_callback callback;
} TermBundle;

TermArgs *ta_new(Hashtable *argdump, void *data)
{
    TermArgs *ta = malloc(sizeof(*ta));
    ta->rules = hst_create();
    ta->extra_help = hst_create();
    ta->rule_order = arr_create(10);

    ta->argdump = argdump;
    ta->data = data;
    ta->seive = NULL;
    ta->maxlen = 0;

    return ta;
}

TermBundle *ta_make_bundle(const char *fmt, const char *help, ta_rule_callback callback)
{
    TermBundle *bun = malloc(sizeof(*bun));
    bun->fmt = fmt;
    bun->help = help;
    bun->callback = callback;

    return bun;
}

void ta_rule(TermArgs *args, const char *rule, const char *fmt, ta_rule_callback callback, const char *help)
{
    if(strcmp(rule, "*"))
    {
        hst_put(&args->rules, (char *)rule, ta_make_bundle(fmt, help, callback), NULL, NULL);

        if(!fmt)
            return;

        size_t len = strlen(fmt);
        if(len > args->maxlen)
            args->maxlen = len;

        arr_append(&args->rule_order, (void *)rule);
    }
    else
        args->seive = callback;
}

void ta_extra(TermArgs *args, const char *fmt, const char *help)
{
    hst_put(&args->extra_help, (char *)fmt, (char *)help, NULL, NULL);
    size_t len = strlen(fmt);
    if(len > args->maxlen)
        args->maxlen = len;
    arr_append(&args->rule_order, (void *)fmt);
}

void ta_run(TermArgs *args, const char *argv[])
{
    while(*argv)
    {
        int skip_next = 0;
        char *arg = (char *)*argv;
        TermBundle *bun = hst_get(&args->rules, arg, NULL, NULL);
        if(!bun)
        {
            if(args->seive)
                args->seive(arg, (char *)*(argv + 1), &skip_next, args->data);
        }
        else
        {
            bun->callback(arg, (char *)*(argv + 1), &skip_next, args->data);
        }

        hst_put(args->argdump, arg, *(argv + 1) ? (char *)*(argv + 1) : (void *)(uintptr_t)1, NULL, NULL);

        argv++;
        if(skip_next && *argv)
            argv++;
    }
}

static void ta_print_spaces(int ct)
{
    char buf[ct + 1];
    memset(buf, ' ', ct);
    buf[ct] = '\0';

    printf("%s", buf);
}

static void ta_print_help(const char *arg, const char *help, size_t maxlen)
{
    size_t alen = strlen(arg);
    printf("  %s", arg);
    ta_print_spaces((maxlen - alen) + 3);
    printf("%s\n", help);
}

void ta_free_each(void *key, void *val, void *data)
{
    free(val);
}

void ta_help(TermArgs *args)
{
    for(int i = 0; i < args->rule_order.count; i++)
    {
        char *arg = args->rule_order.items[i];
        char *help = hst_get(&args->extra_help, arg, NULL, NULL);
        if(help)
        {
            ta_print_help(arg, help, args->maxlen);
            continue;
        }

        TermBundle *bun = hst_get(&args->rules, arg, NULL, NULL);
        if(!bun)
            continue;

        ta_print_help(bun->fmt, bun->help, args->maxlen);
    }
}

void ta_free(TermArgs *args)
{
    hst_for_each(&args->rules, ta_free_each, NULL);
    hst_free(&args->rules);
    hst_free(&args->extra_help);
    arr_free(&args->rule_order);
    free(args);
}

