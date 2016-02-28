/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdlib.h>
#include "variable_manager.h"

#define SCOPE 1
#define BARRIER 2

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
    hst_free(&vs->globals);
}

void vs_push(VarScopeStack *vs)
{
    VarScope *scope = malloc(sizeof(VarScope));
    scope->table = hst_create();
    scope->table.duplicate_keys = 1;
    scope->scope = SCOPE;

    scope->next = vs->scope;

    vs->scope = scope;
}

void vs_pop(VarScopeStack *vs)
{
    VarScope *s = vs->scope;
    if(!s)
        return;

    vs->scope = s->next;

    if(s->scope == SCOPE)
        hst_free(&s->table);
    free(s);
}

VarBundle *vs_get(VarScopeStack *vs, char *ident)
{
    VarScope *s = vs->scope;
    for(; s; s = s->next)
    {
        if(s->scope == BARRIER)
        {
            VarScopeStack temp;
            temp.scope = s->next;

            VarBundle *o = vs_get(&temp, ident);
            if(o)
            {
                VarBarrier *vb = (VarBarrier *)s;
                vb->closedCallback(o, ident, vb->closedData);

                // We expect the callback to alter the scope stack so the
                // following code doesn't recurse forever
                return vs_get(vs, ident);
            }

            return NULL;
        }

        VarBundle *o = hst_get(&s->table, ident, NULL, NULL);
        if(o)
            return o;
    }

    return NULL;
}

void vs_push_closure(VarScopeStack *vs, ClosedCallback cb, void *data)
{
    VarBarrier *barrier = malloc(sizeof(VarBarrier));
    barrier->scope = BARRIER;

    barrier->closedCallback = cb;
    barrier->closedData = data;

    barrier->next = vs->scope;
    vs->scope = (VarScope *)barrier;
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

VarBundle *vs_put(VarScopeStack *vs, char *ident, LLVMValueRef val, EagleTypeType *type)
{
    VarBundle *vb = malloc(sizeof(VarBundle));
    vb->type = type;
    vb->value = val;
    vb->scopeCallback = NULL;
    vb->scopeData = NULL;

    VarScope *s = vs->scope;

    hst_put(&s->table, ident, vb, NULL, NULL);
    pool_add(&vs->pool, vb);

    return vb;
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
