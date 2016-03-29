/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef VARIABLE_MANAGER_H
#define VARIABLE_MANAGER_H

#include "core/hashtable.h"
#include "core/mempool.h"
#include "core/llvm_headers.h"
#include "core/types.h"

typedef void(*LostScopeCallback)(LLVMValueRef, EagleTypeType *, void *);

typedef struct {
    LLVMValueRef value;
    EagleTypeType *type;
    LostScopeCallback scopeCallback;
    void *scopeData;

    unsigned wasused : 1;
    unsigned wasassigned : 1;

    int lineno;

    char *module;
} VarBundle;

typedef void(*ClosedCallback)(VarBundle *, char *, void *);

typedef struct VarScope {
    int scope;
    struct VarScope *next;

    hashtable table;
} VarScope;

typedef struct {
    int scope;
    struct VarScope *next;

    ClosedCallback closedCallback;
    void *closedData;
} VarBarrier;

typedef struct {
    mempool pool;
    VarScope *scope;
    hashtable modules;

    unsigned warnunused : 1;
} VarScopeStack;

VarScopeStack vs_make();
void vs_free(VarScopeStack *vs);
VarBundle *vs_get(VarScopeStack *vs, char *ident);
VarBundle *vs_get_from_module(VarScopeStack *vs, char *ident, char *mod_name);
VarBundle *vs_put(VarScopeStack *vs, char *ident, LLVMValueRef val, EagleTypeType *type, int lineno);
VarBundle *vs_put_in_module(VarScopeStack *vs, char *ident, char *module, LLVMValueRef val, EagleTypeType *type);
void vs_push_closure(VarScopeStack *vs, ClosedCallback cb, void *data);
void vs_push(VarScopeStack *vs);
void vs_pop(VarScopeStack *vs);

void vs_add_callback(VarScopeStack *vs, char *ident, LostScopeCallback callback, void *data);
void vs_run_callbacks_through(VarScopeStack *vs, VarScope *scope);
void vs_run_callbacks_to(VarScopeStack *vs, VarScope *targ);
VarScope *vs_current(VarScopeStack *vs);

#endif
