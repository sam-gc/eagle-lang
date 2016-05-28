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
#include "core/arraylist.h"
#include "core/mempool.h"
#include "core/llvm_headers.h"
#include "core/types.h"
#include "compiler/ast.h"

typedef void(*LostScopeCallback)(LLVMValueRef, EagleComplexType *, void *);
typedef void(*DefermentCallback)(AST *, void *);

typedef struct {
    LLVMValueRef value;
    EagleComplexType *type;
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

    Hashtable table;
    Arraylist deferments;
} VarScope;

typedef struct {
    int scope;
    struct VarScope *next;

    ClosedCallback closedCallback;
    void *closedData;
} VarBarrier;

typedef struct {
    Mempool pool;
    VarScope *scope;
    Hashtable modules;

    unsigned warnunused : 1;

    DefermentCallback deferCallback;
} VarScopeStack;

VarScopeStack vs_make();
void vs_free(VarScopeStack *vs);
VarBundle *vs_get(VarScopeStack *vs, char *ident);
int vs_is_in_local_scope(VarScopeStack *vs, char *ident);
int vs_is_in_global_scope(VarScopeStack *vs, char *ident);
VarBundle *vs_get_from_module(VarScopeStack *vs, char *ident, char *mod_name);
VarBundle *vs_put(VarScopeStack *vs, char *ident, LLVMValueRef val, EagleComplexType *type, int lineno);
VarBundle *vs_put_in_module(VarScopeStack *vs, char *ident, char *module, LLVMValueRef val, EagleComplexType *type);
void vs_push_closure(VarScopeStack *vs, ClosedCallback cb, void *data);
void vs_push(VarScopeStack *vs);
void vs_pop(VarScopeStack *vs);

void vs_add_callback(VarScopeStack *vs, char *ident, LostScopeCallback callback, void *data);
void vs_run_callbacks_through(VarScopeStack *vs, VarScope *scope);
void vs_run_callbacks_to(VarScopeStack *vs, VarScope *targ);
void vs_add_deferment(VarScopeStack *vs, AST *ast);
void vs_run_deferments(VarScopeStack *vs, void *data);
void vs_run_deferments_through(VarScopeStack *vs, VarScope *scope, void *data);
VarScope *vs_current(VarScopeStack *vs);

#endif
