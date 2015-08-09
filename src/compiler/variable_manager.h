#ifndef VARIABLE_MANAGER_H
#define VARIABLE_MANAGER_H

#include "core/hashtable.h"
#include "core/mempool.h"
#include "core/llvm_headers.h"
#include "core/types.h"

typedef struct {
    LLVMValueRef value;
    EagleTypeType *type;
} VarBundle;

typedef struct VarScope {
    struct VarScope *next;
    hashtable table;
} VarScope;

typedef struct {
    mempool pool;
    VarScope *scope;
    hashtable globals;
} VarScopeStack;

VarScopeStack vs_make();
void vs_free(VarScopeStack *vs);
VarBundle *vs_get(VarScopeStack *vs, char *ident);
void vs_put(VarScopeStack *vs, char *ident, LLVMValueRef val, EagleTypeType *type);
void vs_put_global(VarScopeStack *vs, char *ident, LLVMValueRef val, EagleTypeType *type);
VarBundle *vs_get_global(VarScopeStack *vs, char *ident);
void vs_push(VarScopeStack *vs);
void vs_pop(VarScopeStack *vs);

#endif