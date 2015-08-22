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
    hashtable globals;
} VarScopeStack;

VarScopeStack vs_make();
void vs_free(VarScopeStack *vs);
VarBundle *vs_get(VarScopeStack *vs, char *ident);
VarBundle *vs_put(VarScopeStack *vs, char *ident, LLVMValueRef val, EagleTypeType *type);
void vs_put_global(VarScopeStack *vs, char *ident, LLVMValueRef val, EagleTypeType *type);
VarBundle *vs_get_global(VarScopeStack *vs, char *ident);
void vs_push_closure(VarScopeStack *vs, ClosedCallback cb, void *data);
void vs_push(VarScopeStack *vs);
void vs_pop(VarScopeStack *vs);

void vs_add_callback(VarScopeStack *vs, char *ident, LostScopeCallback callback, void *data);
void vs_run_callbacks_through(VarScopeStack *vs, VarScope *scope);
void vs_run_callbacks_to(VarScopeStack *vs, VarScope *targ);
VarScope *vs_current(VarScopeStack *vs);

#endif
