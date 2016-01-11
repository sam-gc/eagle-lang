#include "exports.h"
#include "core/regex.h"
#include <stdlib.h>
#include <string.h>

typedef struct node
{
    struct node *next;
    rgx_regex *rgx;
} rgxnode;

struct export_control
{
    rgxnode *head;
    rgxnode *tail;
};

export_control *ec_alloc()
{
    export_control *ec = malloc(sizeof(export_control));
    ec->head = NULL;
    ec->tail = NULL;

    return ec;
}

void ec_add_str(export_control *ec, const char *str)
{
    rgxnode *node = malloc(sizeof(rgxnode));
    node->next = NULL;
    node->rgx = rgx_compile((char *)str);

    if(!ec->head)
    {
        ec->head = node;
        ec->tail = node;
        return;
    }

    ec->tail->next = node;
    ec->tail = node;
}

int ec_allow(export_control *ec, const char *str)
{
    rgxnode *node;
    for(node = ec->head; node; node = node->next)
    {
        if(rgx_matches(node->rgx, (char *)str))
            return 1;
    }

    return 0;
}

void ec_free(export_control *ec)
{
    rgxnode *node;
    while(node)
    {
        rgxnode *next = node->next;
        rgx_free(node->rgx);
        free(node);
        node = next;
    }

    free(ec);
}

