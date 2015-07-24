/* Lanky -- Scripting Language and Virtual Machine
 * Copyright (C) 2014  Sam Olsen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
