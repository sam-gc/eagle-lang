#include <stdlib.h>
#include <string.h>
#include <stdint.h>

void __egl_incr_ptr(int64_t *ptr)
{
    if(ptr)
        *ptr = *ptr + 1;
}

void __egl_decr_ptr(int64_t *ptr)
{
    if(!ptr)
        return;

    *ptr = *ptr - 1;
    if(!*ptr)
        free(ptr);
}

void __egl_check_ptr(int64_t *ptr)
{
    if(!ptr)
        return;

    if(!*ptr)
        free(ptr);
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

