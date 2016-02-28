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
} arraylist;

typedef enum {
    SORT_RESULT_REVERSE,
    SORT_RESULT_EQUAL,
    SORT_RESULT_SORTED
} arr_sort_result;

typedef char(*arr_pointer_function)(void *);
typedef arr_sort_result (*arr_sort_function)(void *, void *, void *);

arraylist arr_create(long count);
void arr_append(arraylist *list, void *item);
void arr_insert(arraylist *list, void *item, long idx);
void *arr_get(arraylist *list, long idx);
void arr_set(arraylist *list, void *item, long idx);
void arr_sort(arraylist *list, arr_sort_function sf, void *data);
void arr_remove(arraylist *list, void *item, long idx);
void arr_clear(arraylist *list);
long arr_length(arraylist *list);
long arr_index_of(arraylist *list, void *obj);
void arr_for_each(arraylist *list, arr_pointer_function callback);
void arr_free(arraylist *list);

#endif
