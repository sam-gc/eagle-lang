/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ARRAYLIST_H
#define ARRAYLIST_H

typedef struct {
    void **items;
    long count;
    long allocated;
} Arraylist;

typedef enum {
    SORT_RESULT_REVERSE,
    SORT_RESULT_EQUAL,
    SORT_RESULT_SORTED
} arr_sort_result;

typedef char(*arr_pointer_function)(void *);
typedef arr_sort_result (*arr_sort_function)(void *, void *, void *);

Arraylist arr_create(long count);
void arr_append(Arraylist *list, void *item);
void arr_insert(Arraylist *list, void *item, long idx);
void *arr_get(Arraylist *list, long idx);
void arr_set(Arraylist *list, void *item, long idx);
void arr_sort(Arraylist *list, arr_sort_function sf, void *data);
void arr_remove(Arraylist *list, void *item, long idx);
void arr_clear(Arraylist *list);
long arr_length(Arraylist *list);
long arr_index_of(Arraylist *list, void *obj);
void arr_for_each(Arraylist *list, arr_pointer_function callback);
void arr_free(Arraylist *list);

#endif
