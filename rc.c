
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    int64_t memcount;
    int16_t wrefct;
    int16_t wrefal;
    void ***wrefs;
    void (*teardown)(void *, int);
} __egl_ptr;

void __egl_prepare(__egl_ptr *ptr)
{
    ptr->memcount = 0;
    ptr->wrefct = 0;
    ptr->wrefal = 0;
    ptr->wrefs = NULL;
    ptr->teardown = NULL;
}

void __egl_incr_ptr(__egl_ptr *ptr)
{
    if(ptr)
        ptr->memcount = ptr->memcount + 1;
}

void __egl_set_nil(void ***vals, int16_t ct)
{
    int i;
    for(i = 0; i < ct; i++)
        *vals[i] = NULL;
}

void __egl_decr_ptr(__egl_ptr *ptr)
{
    if(!ptr)
        return;

    ptr->memcount = ptr->memcount - 1;
    if(!ptr->memcount)
    {
        if(ptr->wrefs)
        {
            __egl_set_nil(ptr->wrefs, ptr->wrefct);
            free(ptr->wrefs);
        }
        if(ptr->teardown) ptr->teardown(ptr, 1);
        free(ptr);
    }
}

void __egl_check_ptr(__egl_ptr *ptr)
{
    if(!ptr)
        return;

    if(!ptr->memcount)
    {
        if(ptr->wrefs)
        {
            __egl_set_nil(ptr->wrefs, ptr->wrefct);
            free(ptr->wrefs);
        }
        if(ptr->teardown) ptr->teardown(ptr, 1);
        free(ptr);
    }
}

void __egl_add_weak(__egl_ptr *ptr, void **pos)
{
    if(!ptr)
        return;

    if(ptr->wrefal == ptr->wrefct)
    {
        ptr->wrefs = realloc(ptr->wrefs, (ptr->wrefct + 10) * sizeof(void *));
        ptr->wrefal += 10;
    }

    ptr->wrefs[ptr->wrefct++] = pos;
}

void __egl_remove_weak(__egl_ptr **pos)
{
    if(!pos)
        return;
    if(!*pos)
        return;

    __egl_ptr *ptr = *pos;

    int idx = -1;
    int i;
    for(i = 0; i < ptr->wrefct; i++)
    {
        if(ptr->wrefs[i] == (void *)pos)
        {
            idx = i;
            break;
        }
    }

    if(idx < 0)
        return;
    if(idx == ptr->wrefct - 1)
    {
        ptr->wrefct--;
        ptr->wrefs[idx] = NULL;
        return;
    }

    memmove(pos + idx, pos + idx + 1, (ptr->wrefct - idx - 1) * sizeof(void *));
    ptr->wrefs[--ptr->wrefct] = NULL;
}

void __egl_array_fill_nil(void *arr, int64_t ct)
{
    memset(arr, 0, ct * sizeof(void *));
}

void __egl_array_decr_ptrs(void **arr, int64_t ct)
{
    int64_t i;
    for(i = 0; i < ct; i++)
        __egl_decr_ptr(arr[i]);
}

