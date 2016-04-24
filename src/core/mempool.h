/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef MEMPOOL_H
#define MEMPOOL_H

struct poolnode {
    struct poolnode *next;
    void *data;
};

typedef struct {
    struct poolnode *head;
    void (*free_func)(void *);
    struct poolnode *tail;
} Mempool;

Mempool pool_create();
void pool_add(Mempool *pool, void *obj);
void pool_drain(Mempool *pool);

#endif
