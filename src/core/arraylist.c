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

#include "arraylist.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

arraylist arr_create(long count)
{
    arraylist list;
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

void arr_manage_size(arraylist *list)
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

void arr_append(arraylist *list, void *item)
{
    arr_manage_size(list);

    long count = list->count;

    list->items[count++] = item;
    
    if(count > 0 && !list->items[0])
    {
        printf("WTF\n");
    }
    
    list->count = count;
}

void arr_insert(arraylist *list, void *item, long idx)
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

void *arr_get(arraylist *list, long idx)
{
    return list->items[idx];
}

void arr_set(arraylist *list, void *item, long idx)
{
    list->items[idx] = item;
}

void remove_at_index(arraylist *list, long idx)
{
    long i;
    for(i = idx; i < list->count; i++)
    {
        list->items[i] = i < list->count - 1 ? list->items[i + 1] : NULL;
    }

    list->count--;
}

void arr_clear(arraylist *list)
{
    memset(list->items, 0, sizeof(void *) * list->count);
    list->count = 0;
}

void arr_remove(arraylist *list, void *item, long idx)
{
    if(!item)
    {
        remove_at_index(list, idx);
        return;
    }

    idx = arr_index_of(list, item);
    remove_at_index(list, idx);
}

long arr_index_of(arraylist *list, void *obj)
{
    long i;
    for(i = 0; i < list->count; i++)
    {
        if(obj == list->items[i])
            return i;
    }

    return -1;
}

void arr_for_each(arraylist *list, arr_pointer_function callback)
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

long arr_length(arraylist *list)
{
    return list->count;
}

void arr_free(arraylist *list)
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

void arr_sort(arraylist *list, arr_sort_function sf, void *data)
{
    arr_heapsort(list->items, list->count, sf, data);
}

