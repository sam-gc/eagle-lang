/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "multibuffer.h"
#include <string.h>
#include <stdlib.h>

#define MBFILE 0
#define MBSTR  1

#define BUFLEN 500

typedef struct Mbnode {
    struct Mbnode *next;
    char type;
    union {
        FILE *f;
        char *s;
    } src;

    // For string buffers
    size_t slen;
    size_t bufpt;

} Mbnode;

struct Multibuffer {
    Mbnode *head;
    Mbnode *cur;
};

Multibuffer *mb_alloc()
{
    Multibuffer *buf = malloc(sizeof(Multibuffer));
    buf->head = NULL;
    buf->cur = NULL;

    return buf;
}

Mbnode *mb_create_node(char type, const char *data)
{
    Mbnode *node = malloc(sizeof(Mbnode));
    node->next = NULL;
    node->type = type;

    switch(type)
    {
        case MBFILE:
            node->src.f = fopen(data, "r");
            break;
        case MBSTR:
            node->src.s = strdup(data);
            node->slen = strlen(data);
            node->bufpt = 0;
            break;
    }

    return node;
}

void mb_append_node(Mbnode *head, Mbnode *next)
{
    for(; head->next; head = head->next);
    head->next = next;
}

void mb_add_reset(Multibuffer *buf)
{
    Mbnode *next = mb_create_node(MBSTR, "=== RESET ===");
    mb_append_node(buf->head, next);
}

void mb_add_file(Multibuffer *buf, const char *filename)
{
    Mbnode *next = mb_create_node(MBFILE, filename);

    if(buf->head)
    {
        mb_add_reset(buf);
        mb_append_node(buf->head, next);
    }
    else
    {
        buf->head = next;
        buf->cur = next;
    }
}

void mb_add_str(Multibuffer *buf, const char *c)
{
    Mbnode *next = mb_create_node(MBSTR, c);

    if(buf->head)
    {
        mb_add_reset(buf);
        mb_append_node(buf->head, next);
    }
    else
    {
        buf->head = next;
        buf->cur = next;
    }
}

char *mb_get_first_str(Multibuffer *buf)
{
    if(!buf->head)
        return NULL;
    if(buf->head->type == MBFILE)
        return NULL;

    return buf->head->src.s;
}

size_t min(size_t a, size_t b)
{
    return a < b ? a : b;
}

extern int yylineno;
int mb_buffer(Multibuffer *buf, char *dest, size_t max_size)
{
    Mbnode *n = buf->cur;
    if(!n)
        return 0;

    size_t left = max_size;
    size_t loc = 0;
    while(left)
    {
        switch(n->type)
        {
            case MBFILE:
            {
                size_t ct = fread(dest + loc, 1, left, n->src.f);
                if(ct == left)
                    return loc + ct;
                loc += ct;
                left -= ct;
                break;
            }
            case MBSTR:
            {
                size_t lenl = n->slen - n->bufpt;
                size_t ct = min(lenl, left);
                memcpy(dest + loc, n->src.s + n->bufpt, ct);

                n->bufpt += ct;
                if(ct == left)
                    return loc + ct;

                loc += ct;
                left -= ct;
                break;
            }
        }

        n = n->next;
        if(!n)
            return loc;

    }

    return 1;
}

void mb_rewind(Multibuffer *buf)
{
    Mbnode *n = buf->head;
    buf->cur = n;

    for(; n; n = n->next)
    {
        switch(n->type)
        {
            case MBFILE:
                rewind(n->src.f);
                break;
            case MBSTR:
                n->bufpt = 0;
                break;
        }
    }

}

void mb_print_all(Multibuffer *buf)
{
    Mbnode *n = buf->head;

    char fbuf[BUFLEN];

    for(; n; n = n->next)
    {
        switch(n->type)
        {
            case MBFILE:
                while((fgets(fbuf, BUFLEN, n->src.f)) != NULL)
                    printf("%s", fbuf);
                break;
            case MBSTR:
                printf("%s", n->src.s);
                break;
        }
    }
}

void mb_free(Multibuffer *buf)
{
    Mbnode *n = buf->head;
    while(n)
    {
        Mbnode *o = n->next;
        switch(n->type)
        {
            case MBFILE:
                fclose(n->src.f);
                break;
            case MBSTR:
                free(n->src.s);
                break;
        }
        free(n);
        n = o;
    }

    free(buf);
}
