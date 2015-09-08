//
//  types.c
//  Eagle
//
//  Created by Sam Olsen on 7/22/15.
//  Copyright (c) 2015 Sam Olsen. All rights reserved.
//

#include <stdlib.h>
#include <string.h>
#include "hashtable.h"
#include "types.h"

#define TTEST(t, targ, out) if(!strcmp(t, targ)) return ett_base_type(out)
#define ETEST(t, a, b) if(a == b) return t
#define PLACEHOLDER ((void *)1)

LLVMModuleRef the_module = NULL;
LLVMTargetDataRef etTargetData = NULL;

EagleTypeType *et_parse_string(char *text)
{
    TTEST(text, "any", ETAny);
    TTEST(text, "bool", ETInt1);
    TTEST(text, "byte", ETInt8);
    TTEST(text, "int", ETInt32);
    TTEST(text, "long", ETInt64);
    TTEST(text, "double", ETDouble);
    TTEST(text, "void", ETVoid);

    if(ty_is_name(text))
    {
        return ett_struct_type(text);
    }
    
    return NULL;
}

EagleType et_promotion(EagleType left, EagleType right)
{
    if(left == ETNone || left == ETVoid || right == ETNone || right == ETVoid)
        return ETNone;

    return left > right ? left : right;
}

/*
LLVMTypeRef et_llvm_type(EagleType type)
{
    switch(type)
    {
        case ETVoid:
            return LLVMVoidType();
        case ETDouble:
            return LLVMDoubleType();
        case ETInt32:
            return LLVMInt32Type();
        case ETInt64:
            return LLVMInt64Type();
        default:
            return NULL;
    }
}
*/

LLVMTypeRef ett_closure_type(EagleTypeType *type)
{
    if(!ET_IS_CLOSURE(type))
        return NULL;

    EagleFunctionType *ft = (EagleFunctionType *)type;

    LLVMTypeRef *tys = malloc(sizeof(LLVMTypeRef) * (ft->pct + 1));
    int i;
    for(i = 1; i < ft->pct + 1; i++)
        tys[i] = ett_llvm_type(ft->params[i - 1]);
    tys[0] = LLVMPointerType(LLVMInt8Type(), 0);

    LLVMTypeRef out = LLVMFunctionType(ett_llvm_type(ft->retType), tys, ft->pct + 1, 0);
    free(tys);
    return out;
}

LLVMTypeRef ett_llvm_type(EagleTypeType *type)
{
    switch(type->type)
    {
        case ETVoid:
            return LLVMVoidType();
        case ETDouble:
            return LLVMDoubleType();
        case ETInt1:
            return LLVMInt1Type();
        case ETAny:
        case ETInt8:
            return LLVMInt8Type();
        case ETInt32:
            return LLVMInt32Type();
        case ETInt64:
            return LLVMInt64Type();
        case ETStruct:
        {
            EagleStructType *st = (EagleStructType *)type;
            LLVMTypeRef loaded = LLVMGetTypeByName(the_module, st->name);
            if(loaded)
                return loaded;

            return NULL;
           // LLVMTypeRef ty = LLVMStructType(
        }
        case ETPointer:
        {
            EaglePointerType *pt = (EaglePointerType *)type;
            if(pt->counted || pt->weak)
            {
                LLVMTypeRef ptmp[2];
                ptmp[0] = LLVMPointerType(LLVMInt8Type(), 0);
                ptmp[1] = LLVMInt1Type();

                LLVMTypeRef tys[6];
                tys[0] = LLVMInt64Type();
                tys[1] = LLVMInt16Type();
                tys[2] = LLVMInt16Type();
                tys[3] = LLVMPointerType(LLVMInt8Type(), 0);
                tys[4] = LLVMPointerType(LLVMFunctionType(LLVMVoidType(), ptmp, 2, 0), 0);
                tys[5] = ett_llvm_type(pt->to);

                return LLVMPointerType(ty_get_counted(LLVMStructType(tys, 6, 0)), 0);
            }
            return LLVMPointerType(ett_llvm_type(((EaglePointerType *)type)->to), 0);
        }
        case ETArray:
            {
                EagleArrayType *at = (EagleArrayType *)type;
                if(at->ct < 0)
                    return LLVMPointerType(ett_llvm_type(at->of), 0);
                else
                    return LLVMArrayType(ett_llvm_type(at->of), at->ct);
            }
        case ETFunction:
            {
                EagleFunctionType *ft = (EagleFunctionType *)type;
                if(ET_IS_CLOSURE(type))
                {
                    LLVMTypeRef tys[2];
                    tys[0] = LLVMPointerType(LLVMInt8Type(), 0);
                    tys[1] = LLVMPointerType(LLVMInt8Type(), 0);
                    return LLVMStructType(tys, 2, 0);
                }

                LLVMTypeRef *tys = malloc(sizeof(LLVMTypeRef) * ft->pct);
                int i;
                for(i = 0; i < ft->pct; i++)
                    tys[i] = ett_llvm_type(ft->params[i]);
                LLVMTypeRef out = LLVMFunctionType(ett_llvm_type(ft->retType), tys, ft->pct, 0);
                free(tys);
                return out;
            }
        default:
            return NULL;
    }
}

EagleType et_eagle_type(LLVMTypeRef ty)
{
    ETEST(ETDouble, ty, LLVMDoubleType());
    ETEST(ETInt32, ty, LLVMInt32Type());
    ETEST(ETInt64, ty, LLVMInt64Type());
    ETEST(ETVoid, ty, LLVMVoidType());

    return ETNone;
}

EagleTypeType *ett_base_type(EagleType type)
{
    EagleTypeType *ett = malloc(sizeof(EagleTypeType));
    ett->type = type;

    return ett;
}

EagleTypeType *ett_pointer_type(EagleTypeType *to)
{
    EaglePointerType *ett = malloc(sizeof(EaglePointerType));
    ett->type = ETPointer;
    ett->to = to;
    ett->counted = 0;
    ett->weak = 0;
    ett->closed = 0;

    return (EagleTypeType *)ett;
}

EagleTypeType *ett_array_type(EagleTypeType *of, int ct)
{
    EagleArrayType *ett = malloc(sizeof(EagleArrayType));
    ett->type = ETArray;
    ett->of = of;
    ett->ct = ct;

    return (EagleTypeType *)ett;
}

EagleTypeType *ett_function_type(EagleTypeType *retVal, EagleTypeType **params, int pct)
{
    EagleFunctionType *ett = malloc(sizeof(EagleFunctionType));
    ett->type = ETFunction;
    ett->retType = retVal;
    ett->params = malloc(sizeof(EagleTypeType *) * pct);
    memcpy(ett->params, params, sizeof(EagleTypeType *) * pct);
    ett->pct = pct;
    ett->closure = NO_CLOSURE;

    return (EagleTypeType *)ett;
}

EagleTypeType *ett_struct_type(char *name)
{
    EagleStructType *ett = malloc(sizeof(EagleStructType));
    ett->type = ETStruct;
    ett->types = arr_create(10);
    ett->names = arr_create(10);
    ett->name = name;

    return (EagleTypeType *)ett;
}

void ett_struct_add(EagleTypeType *ett, EagleTypeType *ty, char *name)
{
    EagleStructType *e = (EagleStructType *)ett;
    arr_append(&e->types, ty);
    arr_append(&e->names, name);
}

EagleType ett_get_base_type(EagleTypeType *type)
{
    if(type->type == ETPointer)
        return ett_get_base_type(((EaglePointerType *)type)->to);

    return type->type;
}

int ett_are_same(EagleTypeType *left, EagleTypeType *right)
{
    if(left->type != right->type)
        return 0;

    EagleType theType = left->type;
    if(theType == ETPointer)
    {
        EaglePointerType *pl = (EaglePointerType *)left;
        EaglePointerType *pr = (EaglePointerType *)right;

        int cl = pl->counted + pl->weak;
        int cr = pr->counted + pr->weak;
        if(cl > 0 && !cr)
            return 0;
        if(cr > 0 && !cl)
            return 0;

        return ett_are_same(pl->to, pr->to);
    }

    if(theType == ETArray)
    {
        EagleArrayType *al = (EagleArrayType *)left;
        EagleArrayType *ar = (EagleArrayType *)right;

        return ett_are_same(al->of, ar->of) && al->ct == ar->ct;
    }

    return 1;
}

int ett_pointer_depth(EagleTypeType *t)
{
    EaglePointerType *pt = (EaglePointerType *)t;
    int i;
    for(i = 0; pt->type == ETPointer; pt = (EaglePointerType *)pt->to, i++);
    return i;
}

int ett_is_numeric(EagleTypeType *t)
{
    EagleType type = t->type;
    switch(type)
    {
        case ETInt1:
        case ETInt8:
        case ETInt32:
        case ETInt64:
        case ETDouble:
            return 1;
        default:
            return 0;
    }
}

int ett_size_of_type(EagleTypeType *t)
{
    return LLVMStoreSizeOfType(etTargetData, ett_llvm_type(t));
}

void ett_debug_print(EagleTypeType *t)
{
    switch(t->type)
    {
        case ETInt1:
            printf("Bool\n");
            return;
        case ETInt8:
            printf("Byte\n");
            return;
        case ETInt32:
            printf("Int\n");
            return;
        case ETInt64:
            printf("Long\n");
            return;
        case ETDouble:
            printf("Double\n");
            return;
        case ETVoid:
            printf("Void\n");
            return;
        case ETAny:
            printf("Any\n");
            return;
        case ETPointer:
            printf("Pointer to ");
            ett_debug_print(((EaglePointerType *)t)->to);
            return;
        case ETArray:
            printf("Array len(%d) of ", ((EagleArrayType *)t)->ct);
            ett_debug_print(((EagleArrayType *)t)->of);
            return;
        default:
            return;
    }

    return;
}

int ett_array_has_counted(EagleTypeType *t)
{
    EagleArrayType *at = (EagleArrayType *)t;
    if(at->type == ETArray && at->ct > 0)
        return ett_array_has_counted(at->of);

    EaglePointerType *pt = (EaglePointerType *)t;
    return pt->type == ETPointer && pt->counted;
}

int ett_array_count(EagleTypeType *t)
{
    EagleArrayType *at = (EagleArrayType *)t;
    if(at->type == ETArray && at->ct > 0)
        return at->ct * ett_array_count(at->of);

    return 1;
}

static hashtable name_table;
static hashtable struct_table;
static hashtable types_table;
static hashtable counted_table;
void ty_prepare()
{
    name_table = hst_create();
    name_table.duplicate_keys = 1;

    struct_table = hst_create();
    struct_table.duplicate_keys = 1;

    types_table = hst_create();
    types_table.duplicate_keys = 1;

    counted_table = hst_create();
    counted_table.duplicate_keys = 1;
}

void ty_add_name(char *name)
{
    hst_put(&name_table, name, PLACEHOLDER, NULL, NULL);
}

int ty_is_name(char *name)
{
    return (int)hst_get(&name_table, name, NULL, NULL);
}

void ty_teardown()
{
    hst_free(&name_table);
    hst_free(&struct_table);
    hst_free(&types_table);
    hst_free(&counted_table);
}

void ty_add_struct_def(char *name, arraylist *names, arraylist *types)
{
    arraylist *copy = malloc(sizeof(arraylist));
    memcpy(copy, names, sizeof(arraylist));
    hst_put(&struct_table, name, copy, NULL, NULL);

    copy = malloc(sizeof(arraylist));
    memcpy(copy, types, sizeof(arraylist));
    hst_put(&types_table, name, copy, NULL, NULL);
}

void ty_struct_member_index(EagleTypeType *ett, char *member, int *index, EagleTypeType **type)
{
    EagleStructType *st = (EagleStructType *)ett;
    arraylist *names = hst_get(&struct_table, st->name, NULL, NULL);
    arraylist *types = hst_get(&types_table, st->name, NULL, NULL);

    if(!names)
    {
        *type = NULL;
        *index = -2;
        return;
    }

    int i;
    for(i = 0; i < names->count; i++)
    {
        char *tmp = names->items[i];
        if(!strcmp(tmp, member))
        {
            *index = i;
            *type = types->items[i];
            return;
        }
    }

    *type = NULL;
    *index = -1;
}

int ty_needs_destructor(EagleTypeType *ett)
{
    EagleStructType *st = (EagleStructType *)ett;
    arraylist *types = hst_get(&types_table, st->name, NULL, NULL);

    if(!types)
        return -2;

    int i;
    for(i = 0; i < types->count; i++)
    {
        EagleTypeType *ty = types->items[i];
        if(ET_IS_COUNTED(ty) || ET_IS_WEAK(ty))
            return 1;
        if(ty->type == ETStruct)
            return ty_needs_destructor(ty);
    }

    return 0;
}

LLVMTypeRef ty_get_counted(LLVMTypeRef in)
{
    char *translated = LLVMPrintTypeToString(in);
    LLVMTypeRef ref = hst_get(&counted_table, translated, NULL, NULL);

    if(!ref)
    {
        ref = LLVMStructCreateNamed(LLVMGetGlobalContext(), "");
        LLVMTypeRef tys[6];
        LLVMGetStructElementTypes(in, tys);

        LLVMStructSetBody(ref, tys, 6, 0);
        hst_put(&counted_table, translated, ref, NULL, NULL);
    }

    LLVMDisposeMessage(translated);

    return ref;
}
