/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "exports.h"
#include "core/regex.h"
#include <stdlib.h>
#include <string.h>
#include <fnmatch.h>

typedef enum
{
    EREGX,
    EWILD
} rgxnodetype;

typedef struct node
{
    rgxnodetype type;
    struct node *next;
    int token;

    union
    {
        rgx_regex *rgx;
        char *wild;
    } pattern;

} rgxnode;

struct export_control
{
    rgxnode *head;
    rgxnode *tail;
};

export_control *ec_alloc()
{
    export_control *ec = malloc(sizeof(export_control));
    ec->head = NULL;
    ec->tail = NULL;

    return ec;
}

void ec_add_str(export_control *ec, const char *str, int token)
{
    rgxnode *node = malloc(sizeof(rgxnode));
    node->next = NULL;
    node->pattern.rgx = rgx_compile((char *)str);
    node->type = EREGX;
    node->token = token;

    if(!ec->head)
    {
        ec->head = node;
        ec->tail = node;
        return;
    }

    ec->tail->next = node;
    ec->tail = node;
}

void ec_add_wcard(export_control *ec, const char *str, int token)
{
    rgxnode *node = malloc(sizeof(rgxnode));
    node->next = NULL;
    node->pattern.wild = strdup(str);
    node->type = EWILD;
    node->token = token;

    if(!ec->head)
    {
        ec->head = node;
        ec->tail = node;
        return;
    }

    ec->tail->next = node;
    ec->tail = node;
}

int ec_allow(export_control *ec, const char *str, int token)
{
    rgxnode *node;
    for(node = ec->head; node; node = node->next)
    {
        if(node->token && token != node->token)
            continue;

        if(node->type == EREGX && rgx_matches(node->pattern.rgx, (char *)str))
            return 1;
        else if(node->type == EWILD && fnmatch(node->pattern.wild, str, FNM_PATHNAME | FNM_PERIOD) == 0)
            return 1;
    }

    return 0;
}

void ec_free(export_control *ec)
{
    rgxnode *node = ec->head;
    while(node)
    {
        rgxnode *next = node->next;
        if(node->type == EREGX)
            rgx_free(node->pattern.rgx);
        else
            free(node->pattern.wild);
        free(node);
        node = next;
    }

    free(ec);
}
