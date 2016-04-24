/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "arraylist.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

Arraylist arr_create(long count)
{
    Arraylist list;
    list.items = malloc(sizeof(void *) * count);
    list.count = 0;
    list.allocated = count;

    long i;
    for(i = 0; i < count; i++)
    {
        list.items[i] = NULL;
    }

    return list;
}

void arr_manage_size(Arraylist *list)
{
    long count = list->count;
    long alloc = list->allocated;
    if(count == alloc)
    {
        long nl = alloc * 2 < 10000 ? alloc * 2 : alloc + 10000;
        list->items = realloc(list->items, sizeof(void *) * nl);
        list->allocated = nl;
    }
}

void arr_append(Arraylist *list, void *item)
{
    arr_manage_size(list);

    long count = list->count;

    list->items[count++] = item;

    list->count = count;
}

void arr_insert(Arraylist *list, void *item, long idx)
{
    arr_manage_size(list);

    long i;
    for(i = list->count; i > idx; i--)
    {
        list->items[i] = list->items[i - 1];
    }

    list->items[idx] = item;
    list->count++;
}

void *arr_get(Arraylist *list, long idx)
{
    return list->items[idx];
}

void arr_set(Arraylist *list, void *item, long idx)
{
    list->items[idx] = item;
}

void remove_at_index(Arraylist *list, long idx)
{
    long i;
    for(i = idx; i < list->count; i++)
    {
        list->items[i] = i < list->count - 1 ? list->items[i + 1] : NULL;
    }

    list->count--;
}

void arr_clear(Arraylist *list)
{
    memset(list->items, 0, sizeof(void *) * list->count);
    list->count = 0;
}

void arr_remove(Arraylist *list, void *item, long idx)
{
    if(!item)
    {
        remove_at_index(list, idx);
        return;
    }

    idx = arr_index_of(list, item);
    remove_at_index(list, idx);
}

long arr_index_of(Arraylist *list, void *obj)
{
    long i;
    for(i = 0; i < list->count; i++)
    {
        if(obj == list->items[i])
            return i;
    }

    return -1;
}

void arr_for_each(Arraylist *list, arr_pointer_function callback)
{
    if(!callback)
        return;

    long i;
    for(i = 0; i < list->count; i++)
    {
        char res = callback(list->items[i]);
        if(!res)
            return;
    }
}

long arr_length(Arraylist *list)
{
    return list->count;
}

void arr_free(Arraylist *list)
{
    free(list->items);
}

// Sifting code implementation of Wikipedia's heapsort pseudocode: http://en.wikipedia.org/wiki/Heapsort
void arr_siftdown(void *arr[], int start, int end, arr_sort_function sf, void *data)
{
    int root = start;

    while(root * 2 + 1 <= end)
    {
        int child = root * 2 + 1;
        int swap = root;

        arr_sort_result ra = sf(arr[swap], arr[child], data);
        if(ra == SORT_RESULT_SORTED)
            swap = child;
        if(child + 1 <= end)
        {
            arr_sort_result rb = sf(arr[swap], arr[child + 1], data);
            if(rb == SORT_RESULT_EQUAL || rb == SORT_RESULT_SORTED)
                swap = child + 1;
        }

        if(swap == root)
            return;

        void *tmp = arr[root];
        arr[root] = arr[swap];
        arr[swap] = tmp;

        root = swap;
    }
}

void arr_heapify(void *arr[], int length, arr_sort_function sf, void *data)
{
    int start = (length - 2) / 2;

    while(start --> 0) // lol
        arr_siftdown(arr, start, length - 1, sf, data);
}

void arr_heapsort(void *arr[], int length, arr_sort_function sf, void *data)
{
    arr_heapify(arr, length, sf, data);

    int end = length - 1;
    while(end >= 0)
    {
        void *tmp = arr[0];
        arr[0] = arr[end];
        arr[end] = tmp;
        arr_siftdown(arr, 0, --end, sf, data);
    }
}

void arr_sort(Arraylist *list, arr_sort_function sf, void *data)
{
    arr_heapsort(list->items, list->count, sf, data);
}
