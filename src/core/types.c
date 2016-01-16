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
#include "mempool.h"
#include "utils.h"

extern void die(int, const char *, ...);

#define TTEST(t, targ, out) if(!strcmp(t, targ)) return ett_base_type(out)
#define ETEST(t, a, b) if(a == b) return t
#define PLACEHOLDER ((void *)1)

LLVMModuleRef the_module = NULL;
LLVMTargetDataRef etTargetData = NULL;

void ty_method_free(void *k, void *v, void *d);
void ty_struct_def_free(void *k, void *v, void *d);

static mempool type_mempool;
static mempool list_mempool;

static hashtable name_table;
static hashtable struct_table;
static hashtable types_table;
static hashtable counted_table;
static hashtable method_table;
static hashtable type_named_table;
static hashtable init_table;
static hashtable interface_table;
static LLVMTypeRef indirect_struct_type = NULL;
static LLVMTypeRef generator_type = NULL;

void list_mempool_free(void *datum)
{
    arraylist *list = datum;
    arr_free(list);
}

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

    method_table = hst_create();
    method_table.duplicate_keys = 1;

    init_table = hst_create();
    init_table.duplicate_keys = 1;

    type_named_table = hst_create();
    type_named_table.duplicate_keys = 1;

    interface_table = hst_create();
    interface_table.duplicate_keys = 1;

    type_mempool = pool_create();
    list_mempool = pool_create();
    list_mempool.free_func = list_mempool_free;
}

void ty_teardown()
{
    hst_free(&name_table);

    hst_for_each(&struct_table, ty_struct_def_free, NULL);
    hst_free(&struct_table);

    hst_for_each(&types_table, ty_struct_def_free, NULL);
    hst_free(&types_table);
    hst_free(&counted_table);

    hst_for_each(&method_table, ty_method_free, NULL);
    hst_free(&method_table);
    hst_free(&init_table);

    hst_free(&interface_table);

    hst_free(&type_named_table);

    pool_drain(&list_mempool);
    pool_drain(&type_mempool);

    indirect_struct_type = NULL;
    generator_type = NULL;
}

EagleTypeType *et_parse_string(char *text)
{
    TTEST(text, "auto", ETAuto);
    TTEST(text, "any", ETAny);
    TTEST(text, "bool", ETInt1);
    TTEST(text, "byte", ETInt8);
    TTEST(text, "short", ETInt16);
    TTEST(text, "int", ETInt32);
    TTEST(text, "long", ETInt64);
    TTEST(text, "double", ETDouble);
    TTEST(text, "void", ETVoid);

    if(ty_is_name(text))
    {
        if(ty_is_class(text))
            return ett_class_type(text);
        else if(ty_is_interface(text))
            return ett_interface_type(text);
        else
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

LLVMTypeRef ett_closure_type(EagleTypeType *type) { if(!ET_IS_CLOSURE(type)) return NULL; 
    EagleFunctionType *ft = (EagleFunctionType *)type;

    LLVMTypeRef *tys = malloc(sizeof(LLVMTypeRef) * (ft->pct + 1));
    int i;
    for(i = 1; i < ft->pct + 1; i++)
        tys[i] = ett_llvm_type(ft->params[i - 1]);
    tys[0] = LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0);

    LLVMTypeRef out = LLVMFunctionType(ett_llvm_type(ft->retType), tys, ft->pct + 1, 0);
    free(tys);
    return out;
}

LLVMTypeRef ett_llvm_type(EagleTypeType *type)
{
    switch(type->type)
    {
        case ETVoid:
            return LLVMVoidTypeInContext(utl_get_current_context());
        case ETDouble:
            return LLVMDoubleTypeInContext(utl_get_current_context());
        case ETInt1:
            return LLVMInt1TypeInContext(utl_get_current_context());
        case ETAny:
        case ETInt8:
            return LLVMInt8TypeInContext(utl_get_current_context());
        case ETInt16:
            return LLVMInt16TypeInContext(utl_get_current_context());
        case ETInt32:
            return LLVMInt32TypeInContext(utl_get_current_context());
        case ETInt64:
            return LLVMInt64TypeInContext(utl_get_current_context());
        case ETCString:
            return LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0);
        case ETGenerator:
        {
            if(generator_type) 
                return generator_type;

            LLVMTypeRef ptmp[2];
            ptmp[0] = LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0);
            ptmp[1] = LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0);

            generator_type = LLVMStructCreateNamed(utl_get_current_context(), "__egl_gen_strct");
            LLVMStructSetBody(generator_type, ptmp, 2, 0);
            return generator_type;
        }
        case ETClass:
        case ETStruct:
        {
            EagleStructType *st = (EagleStructType *)type;
            LLVMTypeRef loaded = LLVMGetTypeByName(the_module, st->name);
            if(loaded)
                return loaded;

            return NULL;
           // LLVMTypeRef ty = LLVMStructTypeInContext(utl_get_current_context(), 
        }
        case ETInterface:
        {
            return LLVMInt8TypeInContext(utl_get_current_context());
        }
        case ETPointer:
        {
            EaglePointerType *pt = (EaglePointerType *)type;
            if(pt->counted || pt->weak)
            {
                LLVMTypeRef ptmp[2];
                ptmp[0] = LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0);
                ptmp[1] = LLVMInt1TypeInContext(utl_get_current_context());

                LLVMTypeRef tys[6];
                tys[0] = LLVMInt64TypeInContext(utl_get_current_context());
                tys[1] = LLVMInt16TypeInContext(utl_get_current_context());
                tys[2] = LLVMInt16TypeInContext(utl_get_current_context());
                tys[3] = LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0);
                tys[4] = LLVMPointerType(LLVMFunctionType(LLVMVoidTypeInContext(utl_get_current_context()), ptmp, 2, 0), 0);
                tys[5] = ett_llvm_type(pt->to);

                return LLVMPointerType(ty_get_counted(LLVMStructTypeInContext(utl_get_current_context(), tys, 6, 0)), 0);
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
                    tys[0] = LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0);
                    tys[1] = LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0);
                    return LLVMStructTypeInContext(utl_get_current_context(), tys, 2, 0);
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
    ETEST(ETDouble, ty, LLVMDoubleTypeInContext(utl_get_current_context()));
    ETEST(ETInt32, ty, LLVMInt32TypeInContext(utl_get_current_context()));
    ETEST(ETInt64, ty, LLVMInt64TypeInContext(utl_get_current_context()));
    ETEST(ETVoid, ty, LLVMVoidTypeInContext(utl_get_current_context()));

    return ETNone;
}

#define TT(t) {ET ## t}
static EagleTypeType base_types[] = {
    TT(None),
    TT(Any),
    TT(Auto),
    TT(Nil),
    TT(Int1),
    TT(Int8),
    TT(Int16),
    TT(Int32),
    TT(Int64),
    TT(Double),
    TT(CString),
    TT(Pointer),
    TT(Array),
    TT(Void),
    TT(Function),
    TT(Generator),
    TT(Struct),
    TT(Class),
};

EagleTypeType *ett_base_type(EagleType type)
{
    return &base_types[type];
}

EagleTypeType *ett_pointer_type(EagleTypeType *to)
{
    EaglePointerType *ett = malloc(sizeof(EaglePointerType));
    ett->type = ETPointer;
    ett->to = to;
    ett->counted = 0;
    ett->weak = 0;
    ett->closed = 0;

    pool_add(&type_mempool, ett);

    return (EagleTypeType *)ett;
}

EagleTypeType *ett_array_type(EagleTypeType *of, int ct)
{
    EagleArrayType *ett = malloc(sizeof(EagleArrayType));
    ett->type = ETArray;
    ett->of = of;
    ett->ct = ct;

    pool_add(&type_mempool, ett);

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
    ett->gen = 0;

    pool_add(&type_mempool, ett);
    pool_add(&type_mempool, ett->params);

    return (EagleTypeType *)ett;
}

EagleTypeType *ett_gen_type(EagleTypeType *ytype)
{
    EagleGenType *ett = malloc(sizeof(EagleGenType));
    ett->type = ETGenerator;
    ett->ytype = ytype;

    pool_add(&type_mempool, ett);

    return (EagleTypeType *)ett;
}

EagleTypeType *ett_struct_type(char *name)
{
    EagleTypeType *et = hst_get(&type_named_table, name, NULL, NULL);
    if(et)
        return et;

    EagleStructType *ett = malloc(sizeof(EagleStructType));
    ett->type = ETStruct;
    ett->types = arr_create(10);
    ett->names = arr_create(10);
    ett->name = name;

    hst_put(&type_named_table, name, ett, NULL, NULL);
    pool_add(&type_mempool, ett);
    pool_add(&list_mempool, &ett->types);
    pool_add(&list_mempool, &ett->names);

    return (EagleTypeType *)ett;
}

EagleTypeType *ett_class_type(char *name)
{
    EagleTypeType *et = hst_get(&type_named_table, name, NULL, NULL);
    if(et)
        return et;

    EagleStructType *ett = malloc(sizeof(EagleStructType));
    ett->type = ETClass;
    ett->types = arr_create(10);
    ett->names = arr_create(10);
    ett->name = name;

    hst_put(&type_named_table, name, ett, NULL, NULL);
    pool_add(&type_mempool, ett);
    pool_add(&list_mempool, &ett->types);
    pool_add(&list_mempool, &ett->names);

    return (EagleTypeType *)ett;
}

EagleTypeType *ett_interface_type(char *name)
{
    EagleTypeType *et = hst_get(&type_named_table, name, NULL, NULL);
    if(et)
        return et;

    EagleInterfaceType *ett = malloc(sizeof(EagleInterfaceType));
    ett->names = arr_create(3);
    arr_append(&ett->names, name);
    ett->type = ETInterface;

    pool_add(&list_mempool, &ett->names);
    pool_add(&type_mempool, ett);

    return (EagleTypeType *)ett;
}

void ett_composite_interface(EagleTypeType *ett, char *name)
{
    EagleInterfaceType *ei = (EagleInterfaceType *)ett;

    arr_append(&ei->names, name);
}

void ett_struct_add(EagleTypeType *ett, EagleTypeType *ty, char *name)
{
    EagleStructType *e = (EagleStructType *)ett;
    arr_append(&e->types, ty);
    arr_append(&e->names, name);
}

void ett_class_set_interfaces(EagleTypeType *ett, arraylist *interfaces)
{
    EagleStructType *st = (EagleStructType *)ett;
    memcpy(&st->interfaces, interfaces, sizeof(arraylist));
}

EagleTypeType *ett_get_root_pointee(EagleTypeType *type)
{
    if(type->type == ETPointer)
        return ett_get_root_pointee(((EaglePointerType *)type)->to);

    return type;
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
    {
        return 0;
    }


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

    // FOR NOW -- Classes are represented with the same C struct
    // as structures. So this works.
    if(theType == ETStruct || theType == ETClass)
    {
        EagleStructType *sl = (EagleStructType *)left;
        EagleStructType *sr = (EagleStructType *)right;
        
        return strcmp(sl->name, sr->name) == 0;
    }

    if(theType == ETFunction)
    {
        EagleFunctionType *fl = (EagleFunctionType *)left;
        EagleFunctionType *fr = (EagleFunctionType *)right;

        if(!ett_are_same(fl->retType, fr->retType))
            return 0;
        if(fl->pct != fr->pct)
            return 0;
        int i;
        for(i = 0; i < fl->pct; i++)
            if(!ett_are_same(fl->params[i], fr->params[i]))
                return 0;
        if((fl->closure && !fr->closure) || (!fl->closure && fr->closure))
            return 0;

        return fl->gen == fr->gen;
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
        case ETInt16:
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

LLVMTypeRef ty_class_indirect()
{
    if(indirect_struct_type)
        return indirect_struct_type;

    indirect_struct_type = LLVMStructCreateNamed(utl_get_current_context(), "__egl_class_indirect");

    LLVMTypeRef tys[] = {
        LLVMInt32TypeInContext(utl_get_current_context()),
        LLVMPointerType(LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), 0),
        LLVMPointerType(LLVMInt64TypeInContext(utl_get_current_context()), 0),
        LLVMPointerType(LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), 0)
    };

    LLVMStructSetBody(indirect_struct_type, tys, 4, 0);
    return indirect_struct_type;
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
        case ETInt16:
            printf("Short\n");
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

void ty_add_name(char *name)
{
    hst_put(&name_table, name, PLACEHOLDER, NULL, NULL);
}

int ty_is_name(char *name)
{
    return (int)(uintptr_t)hst_get(&name_table, name, NULL, NULL);
}

int ty_is_class(char *name)
{
    return (int)(uintptr_t)hst_get(&method_table, name, NULL, NULL);
}

int ty_is_interface(char *name)
{
    return (int)(uintptr_t)hst_get(&interface_table, name, NULL, NULL);
}

void ty_method_free(void *k, void *v, void *d)
{
    hst_free(v);
    free(v);
}

void ty_struct_def_free(void *k, void *v, void *d)
{
    free(v);
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

void ty_register_class(char *name)
{
    hashtable *lst = hst_get(&method_table, name, NULL, NULL);
    if(lst)
        die(-1, "Redeclaring class with name: %s", name);

    lst = malloc(sizeof(hashtable));
    *lst = hst_create();
    lst->duplicate_keys = 1;
    hst_put(&method_table, name, lst, NULL, NULL);
}

void ty_register_interface(char *name)
{
    if(hst_get(&interface_table, name, NULL, NULL))
        die(-1, "Redeclaring interface with name: %s", name);
    if(hst_get(&method_table, name, NULL, NULL))
        die(-1, "Interface declaration %s clashes with previously named class.", name);

    arraylist *list = malloc(sizeof(arraylist));
    *list = arr_create(10);

    hst_put(&interface_table, name, list, NULL, NULL);
}

int ty_interface_offset(char *name, char *method)
{
    arraylist *names = hst_get(&interface_table, name, NULL, NULL);
    if(!names)
        return -1;

    int i;
    for(i = 0; i < names->count; i++)
        if(strcmp(names->items[i], method) == 0)
            return i;
    return -1;
}

int ty_interface_count(char *name)
{
    arraylist *names = hst_get(&interface_table, name, NULL, NULL);
    if(!names)
        return -1;

    return (int)names->count;
}

void ty_add_init(char *name, EagleTypeType *ty)
{
    hst_put(&init_table, name, ty, NULL, NULL);
}

EagleTypeType *ty_get_init(char *name)
{
    return hst_get(&init_table, name, NULL, NULL);
}

void ty_add_interface_method(char *name, char *method, EagleTypeType *ty)
{
    arraylist *names = hst_get(&interface_table, name, NULL, NULL);
    arr_append(names, method);
    ty_add_method(name, method, ty);
}

char *ty_interface_for_method(EagleTypeType *ett, char *method)
{
    EagleInterfaceType *it = (EagleInterfaceType *)ett;
    arraylist *names = &it->names;

    int i;
    for(i = 0; i < names->count; i++)
    {
        char *name = arr_get(names, i);
        if(ty_method_lookup(name, method))
            return name;
    }

    return NULL;
}

int ty_class_implements_interface(EagleTypeType *type, EagleTypeType *interface)
{
    while(type->type == ETPointer)
        type = ET_POINTEE(type);

    while(interface->type == ETPointer)
        interface = ET_POINTEE(interface);

    EagleStructType *st = (EagleStructType *)type;

    arraylist *ifcs = &st->interfaces;
    EagleInterfaceType *ie = (EagleInterfaceType *)interface;

    int j;
    for(j = 0; j < ie->names.count; j++)
    {
        int good = 0;
        char *name = arr_get(&ie->names, j);
        int i;
        for(i = 0; i < ifcs->count; i++)
        {
            if(strcmp(ifcs->items[i], name) == 0)
                good = 1;
        }

        if(!good)
            return 0;
    }

    return 1;
}

void ty_add_method(char *name, char *method, EagleTypeType *ty)
{
    hashtable *lst = hst_get(&method_table, name, NULL, NULL);
    if(!lst)
    {
        lst = malloc(sizeof(hashtable));
        *lst = hst_create();
        lst->duplicate_keys = 1;
        hst_put(&method_table, name, lst, NULL, NULL);
    }

    hst_put(lst, method, ty, NULL, NULL);
}

EagleTypeType *ty_method_lookup(char *name, char *method)
{
    hashtable *lst = hst_get(&method_table, name, NULL, NULL);
    if(!lst)
        return NULL;

    return hst_get(lst, method, NULL, NULL);
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
            *type = types->items[i];
            if(ty_is_class(st->name))
                i += 1;
            *index = i;
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
        if(ty->type == ETArray && ett_array_has_counted(ty))
            return 1;
    }

    return 0;
}

LLVMTypeRef ty_get_counted(LLVMTypeRef in)
{
    char *translated = LLVMPrintTypeToString(in);
    LLVMTypeRef ref = hst_get(&counted_table, translated, NULL, NULL);

    if(!ref)
    {
        ref = LLVMStructCreateNamed(utl_get_current_context(), "");
        LLVMTypeRef tys[6];
        LLVMGetStructElementTypes(in, tys);

        LLVMStructSetBody(ref, tys, 6, 0);
        hst_put(&counted_table, translated, ref, NULL, NULL);
    }

    LLVMDisposeMessage(translated);

    return ref;
}

