#include <stdlib.h>
#include "variable_manager.h"

VarScope vs_make()
{
    VarScope vs;
    vs.pool = pool_create();
    vs.table = hst_create();

    vs.table.duplicate_keys = 1;

    return vs;
}

void vs_free(VarScope *vs)
{
    pool_drain(&vs->pool);
    hst_free(&vs->table);
}

VarBundle *vs_get(VarScope *vs, char *ident)
{
    return (VarBundle *)hst_get(&vs->table, ident, NULL, NULL);
}

void vs_put(VarScope *vs, char *ident, LLVMValueRef val, EagleType type)
{
    VarBundle *vb = malloc(sizeof(VarBundle));
    vb->type = type;
    vb->value = val;

    hst_put(&vs->table, ident, vb, NULL, NULL);
    pool_add(&vs->pool, vb);
}
