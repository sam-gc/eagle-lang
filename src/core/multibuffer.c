#include "multibuffer.h"
#include <string.h>
#include <stdlib.h>

#define MBFILE 0
#define MBSTR  1

typedef struct mbnode {
    struct mbnode *next;
    char type;
    union {
        FILE *f;
        char *s;
    } src;

    // For string buffers
    size_t slen;
    size_t bufpt;
} mbnode;

struct multibuffer {
    mbnode *head;
    mbnode *cur;
};

multibuffer *mb_alloc()
{
    multibuffer *buf = malloc(sizeof(multibuffer));
    buf->head = NULL;
    buf->cur = NULL;

    return buf;
}

mbnode *mb_create_node(char type, const char *data)
{
    mbnode *node = malloc(sizeof(mbnode));
    node->next = NULL;
    node->type = type;

    switch(type)
    {
        case MBFILE:
            node->src.f = fopen(data, "r");
            break;
        case MBSTR:
            node->src.s = strdup(data);
            node->slen = strlen(data);
            node->bufpt = 0;
            break;
    }

    return node;
}

void mb_append_node(mbnode *head, mbnode *next)
{
    for(; head->next; head = head->next);
    head->next = next;
}

void mb_add_file(multibuffer *buf, const char *filename)
{
    mbnode *next = mb_create_node(MBFILE, filename);
    if(buf->head)
        mb_append_node(buf->head, next);
    else
    {
        buf->head = next;
        buf->cur = next;
    }
}

void mb_add_str(multibuffer *buf, const char *c)
{
    mbnode *next = mb_create_node(MBSTR, c);
    if(buf->head)
        mb_append_node(buf->head, next);
    else
    {
        buf->head = next;
        buf->cur = next;
    }
}

char *mb_get_first_str(multibuffer *buf)
{
    if(!buf->head)
        return NULL;
    if(buf->head->type == MBFILE)
        return NULL;

    return buf->head->src.s;
}

size_t min(size_t a, size_t b)
{
    return a < b ? a : b;
}

int mb_buffer(multibuffer *buf, char *dest, size_t max_size)
{
    mbnode *n = buf->cur;
    if(!n)
        return 0;

    size_t left = max_size;
    size_t loc = 0;
    while(left)
    {
        switch(n->type)
        {
            case MBFILE:
            {
                size_t ct = fread(dest + loc, 1, left, n->src.f);
                if(ct == left)
                    return loc + ct;
                loc += ct;
                left -= ct;
                break;
            }
            case MBSTR:
            {
                size_t lenl = n->slen - n->bufpt;
                size_t ct = min(lenl, left);
                memcpy(dest + loc, n->src.s + n->bufpt, ct);
                
                n->bufpt += ct;
                if(ct == left)
                    return loc + ct;

                loc += ct;
                left -= ct;
            }
        }

        n = n->next;
        if(!n)
            return loc;
    }

    return 1;
}

void mb_rewind(multibuffer *buf)
{
    mbnode *n = buf->head;
    buf->cur = n;

    for(; n; n = n->next)
    {
        switch(n->type)
        {
            case MBFILE:
                rewind(n->src.f);
                break;
            case MBSTR:
                n->bufpt = 0;
                break;
        }
    }
}

void mb_free(multibuffer *buf)
{
    mbnode *n = buf->head;
    while(n)
    {
        mbnode *o = n->next;
        switch(n->type)
        {
            case MBFILE:
                fclose(n->src.f);
                break;
            case MBSTR:
                free(n->src.s);
                break;
        }
        free(n);
        n = o;
    }

    free(buf);
}

