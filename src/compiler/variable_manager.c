#include <stdlib.h>
#include "variable_manager.h"

VarScopeStack vs_make()
{
    VarScopeStack vs;
    vs.pool = pool_create();
    vs.scope = NULL;
    vs.globals = hst_create();
    vs.globals.duplicate_keys = 1;

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



void vs_put_global(VarScopeStack *vs, char *ident, LLVMValueRef val, EagleTypeType *type)
{
    VarBundle *vb = malloc(sizeof(VarBundle));
    vb->type = type;
    vb->value = val;

    hst_put(&vs->globals, ident, vb, NULL, NULL);
    pool_add(&vs->pool, vb);
}

VarBundle *vs_get_global(VarScopeStack *vs, char *ident)
{
    return hst_get(&vs->globals, ident, NULL, NULL);
}

void vs_put(VarScopeStack *vs, char *ident, LLVMValueRef val, EagleTypeType *type)
{
    VarBundle *vb = malloc(sizeof(VarBundle));
    vb->type = type;
    vb->value = val;
    vb->scopeCallback = NULL;
    vb->scopeData = NULL;

    VarScope *s = vs->scope;

    hst_put(&s->table, ident, vb, NULL, NULL);
    pool_add(&vs->pool, vb);
}

void vs_add_callback(VarScopeStack *vs, char *ident, LostScopeCallback callback, void *data)
{
    VarScope *s = vs->scope;
    VarBundle *vb = hst_get(&s->table, ident, NULL, NULL);
    if(!vb)
        return;
    
    vb->scopeCallback = callback;
    vb->scopeData = data;
}

void vs_callback_callback(void *key, void *val, void *data)
{
    VarBundle *vb = val;
    if(vb->scopeCallback)
    {
        vb->scopeCallback(vb->value, vb->type, vb->scopeData);
    }
}

void vs_run_callbacks(VarScopeStack *vs, VarScope *targ, int through)
{
    VarScope *scope;
    for(scope = vs->scope; scope && scope != targ; scope = scope->next)
    {
        hst_for_each(&scope->table, vs_callback_callback, NULL);
    }

    if(scope && through)
    {
        hst_for_each(&scope->table, vs_callback_callback, NULL);
    }
}

void vs_run_callbacks_through(VarScopeStack *vs, VarScope *targ)
{
    vs_run_callbacks(vs, targ, 1);
}

void vs_run_callbacks_to(VarScopeStack *vs, VarScope *targ)
{
    vs_run_callbacks(vs, targ, 0);
}

VarScope *vs_current(VarScopeStack *vs)
{
    return vs->scope;
}

