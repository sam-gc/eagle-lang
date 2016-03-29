/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdlib.h>
#include "variable_manager.h"
#include "core/config.h"

#define SCOPE 1
#define BARRIER 2

VarScopeStack vs_make()
{
    VarScopeStack vs;
    vs.pool = pool_create();
    vs.scope = NULL;
    vs.modules = hst_create();
    vs.modules.duplicate_keys = 1;
    vs.warnunused = 1;

    return vs;
}

static void vs_modules_free_each(void *key, void *val, void *data)
{
    hst_free(val);
    free(val);
}

void vs_free(VarScopeStack *vs)
{
    pool_drain(&vs->pool);

    hst_for_each(&vs->modules, &vs_modules_free_each, NULL);
    hst_free(&vs->modules);
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

void vs_run_warnings(void *key, void *val, void *data)
{
    char *ident = key;
    VarBundle *bun = val;

    if(bun->lineno < 0)
        return;

    if(!bun->wasused && !bun->wasassigned)
        warn(bun->lineno, "Unused variable %s", ident);
    else if(!bun->wasused)
        warn(bun->lineno, "Variable %s assigned but never used", ident);
}

void vs_pop(VarScopeStack *vs)
{
    VarScope *s = vs->scope;
    if(!s)
        return;

    vs->scope = s->next;

    if(s->scope == SCOPE)
    {
        if(vs->warnunused)
            hst_for_each(&s->table, vs_run_warnings, NULL);
        hst_free(&s->table);
    }
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

VarBundle *vs_get_from_module(VarScopeStack *vs, char *ident, char *mod_name)
{
    hashtable *module = hst_get(&vs->modules, mod_name, NULL, NULL);
    if(!module)
        return NULL;

    return hst_get(module, ident, NULL, NULL);
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

static VarBundle *vs_create(char *ident, char *module, EagleTypeType *type, LLVMValueRef val, int lineno)
{
    VarBundle *vb = malloc(sizeof(VarBundle));
    vb->type = type;
    vb->value = val;
    vb->scopeCallback = NULL;
    vb->scopeData = NULL;
    vb->wasused = vb->wasassigned = 0;
    vb->lineno = lineno;
    vb->module = module;

    return vb;
}

VarBundle *vs_put(VarScopeStack *vs, char *ident, LLVMValueRef val, EagleTypeType *type, int lineno)
{
    VarBundle *vb = vs_create(ident, NULL, type, val, lineno);
    VarScope *s = vs->scope;

    hst_put(&s->table, ident, vb, NULL, NULL);
    pool_add(&vs->pool, vb);

    return vb;
}

VarBundle *vs_put_in_module(VarScopeStack *vs, char *ident, char *module, LLVMValueRef val, EagleTypeType *type)
{
    VarBundle *vb = vs_create(ident, module, type, val, -1);

    hashtable *mod = hst_get(&vs->modules, module, NULL, NULL);
    if(!mod)
    {
        mod = malloc(sizeof(hashtable));
        *mod = hst_create();
        mod->duplicate_keys = 1;

        hst_put(&vs->modules, module, mod, NULL, NULL);
    }

    hst_put(mod, ident, vb, NULL, NULL);
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
