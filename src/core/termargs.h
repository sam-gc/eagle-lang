/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TERMARGS_H
#define TERMARGS_H

#include <stdlib.h>
#include "hashtable.h"

typedef void (*ta_rule_callback)(char *arg, char *next, int *skip, void *data);

typedef struct
{
    hashtable rules;
    hashtable extra_help;
    hashtable *argdump;
    void *data;

    ta_rule_callback seive;

    size_t maxlen;
} TermArgs;

TermArgs *ta_new(hashtable *argdump, void *data);
void ta_rule(TermArgs *args, const char *rule, const char *fmt, ta_rule_callback callback, const char *help);
void ta_extra(TermArgs *args, const char *fmt, const char *help);
void ta_run(TermArgs *args, const char *argv[]);
void ta_help(TermArgs *args);
void ta_free(TermArgs *args);

#endif

