#include <stdlib.h>
#include "variable_manager.h"

VarScopeStack vs_make()
{
    VarScopeStack vs;
    vs.pool = pool_create();
    vs.scope = NULL;

    return vs;
}

void vs_free(VarScopeStack *vs)
{
    pool_drain(&vs->pool);
}

void vs_push(VarScopeStack *vs)
{
    VarScope *scope = malloc(sizeof(VarScope));
    scope->table = hst_create();
    scope->table.duplicate_keys = 1;

    scope->next = vs->scope;

    vs->scope = scope;
}

void vs_pop(VarScopeStack *vs)
{
    VarScope *s = vs->scope;
    if(!s)
        return;

    vs->scope = s->next;
    hst_free(&s->table);
    free(s);
}

VarBundle *vs_get(VarScopeStack *vs, char *ident)
{
    VarScope *s = vs->scope;
    for(; s; s = s->next)
    {
        VarBundle *o = hst_get(&s->table, ident, NULL, NULL);
        if(o)
            return o;
    }

    return NULL;
}

void vs_put(VarScopeStack *vs, char *ident, LLVMValueRef val, EagleType type)
{
    VarBundle *vb = malloc(sizeof(VarBundle));
    vb->type = type;
    vb->value = val;

    VarScope *s = vs->scope;

    hst_put(&s->table, ident, vb, NULL, NULL);
    pool_add(&vs->pool, vb);
}
