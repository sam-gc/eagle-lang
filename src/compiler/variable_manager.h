#ifndef VARIABLE_MANAGER_H
#define VARIABLE_MANAGER_H

#include "core/hashtable.h"
#include "core/mempool.h"
#include "core/llvm_headers.h"
#include "core/types.h"

typedef struct {
    LLVMValueRef value;
    EagleType type;
} VarBundle;

typedef struct {
    mempool pool;
    hashtable table;
} VarScope;

VarScope vs_make();
void vs_free(VarScope *vs);
VarBundle *vs_get(VarScope *vs, char *ident);
void vs_put(VarScope *vs, char *ident, LLVMValueRef val, EagleType type);

#endif
