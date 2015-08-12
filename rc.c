#include <stdlib.h>
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

