/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "mempool.h"
#include <stdlib.h>
#include <stdint.h>

struct poolnode *gen_node(void *obj)
{
    struct poolnode *node = malloc(sizeof(struct poolnode));
    node->next = NULL;
    node->data = obj;

    return node;
}

void append_to_list(struct poolnode **head, struct poolnode **tail, struct poolnode *next)
{
    if(!*head)
    {
        *head = next;
        *tail = next;
        return;
    }

    (*tail)->next = next;
    *tail = next;
}

mempool pool_create()
{
    mempool pool;
    pool.head = NULL;
    pool.free_func = NULL;
    pool.tail = NULL;

    return pool;
}

void pool_add(mempool *pool, void *obj)
{
    struct poolnode *next = gen_node(obj);
    append_to_list(&(pool->head), &(pool->tail), next);
}

void pool_drain(mempool *pool)
{
    struct poolnode *node = pool->head;
    while(node)
    {
        void *data = node->data;
        if(pool->free_func)
            pool->free_func(data);
        else
            free(data);
        struct poolnode *cur = node;
        node = node->next;
        free(cur);
    }

    pool->head = NULL;
}
