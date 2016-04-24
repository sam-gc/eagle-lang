/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TYPES_H
#define TYPES_H

#include <stdio.h>
#include "llvm_headers.h"
#include "arraylist.h"

#define NO_CLOSURE 0
#define CLOSURE_NO_CLOSE 1
#define CLOSURE_CLOSE 2
#define CLOSURE_RECURSE 3

// #define ET_IS_INT(e) ((e) == ETInt8 || (e) == ETInt32 || (e) == ETInt64)
// #define ET_IS_REAL(e) ((e) == ETDouble)
#define ET_IS_GEN_ARR(e) (((EagleArrayType *)(e))->ct < 0)
#define ET_IS_COUNTED(p) ((p)->type == ETPointer && ((EaglePointerType *)(p))->counted)
#define ET_IS_WEAK(p) ((p)->type == ETPointer && ((EaglePointerType *)(p))->weak)
#define ET_IS_CLOSED(p) ((p)->type == ETPointer && ((EaglePointerType *)(p))->closed)
#define ET_IS_CLOSURE(p) ((p)->type == ETFunction && ((EagleFunctionType *)(p))->closure)
#define ET_HAS_CLOASED(p) ((p)->type == ETFunction && ((EagleFunctionType *)(p))->closure == CLOSURE_CLOSE)
#define ET_IS_RECURSE(p) ((p)->type == ETFunction && ((EagleFunctionType *)(p))->closure == CLOSURE_RECURSE)
#define ET_POINTEE(p) (((EaglePointerType *)(p))->to)
#define ET_IS_RAW_FUNCTION(p) ((p)->type == ETFunction && !((EagleFunctionType *)(p))->closure)

extern LLVMTargetDataRef etTargetData;
extern LLVMModuleRef the_module;

typedef enum {
    ETNone = 0,
    ETAny,
    ETAuto,
    ETNil,
    ETInt1,
    ETInt8,
    ETInt16,
    ETInt32,
    ETInt64,
    ETFloat,
    ETDouble,
    ETUInt8,
    ETUInt16,
    ETUInt32,
    ETUInt64,
    ETCString,
    ETPointer,
    ETArray,
    ETVoid,
    ETFunction,
    ETGenerator,
    ETStruct,
    ETClass,
    ETInterface,
    ETEnum
} EagleBasicType;

typedef struct {
    EagleBasicType type;
} EagleComplexType;

typedef struct {
    EagleBasicType type;
    EagleComplexType *to;
    int counted;
    int weak;
    int closed;
} EaglePointerType;

typedef struct {
    EagleBasicType type;
    EagleComplexType *of;
    int ct;
} EagleArrayType;

typedef struct {
    EagleBasicType type;
    EagleComplexType *retType;
    EagleComplexType **params;
    int pct;
    int variadic;

    int closure;
    int gen;
} EagleFunctionType;

typedef struct {
    EagleBasicType type;
    EagleComplexType *ytype;
} EagleGenType;

typedef struct {
    EagleBasicType type;
    Arraylist types;
    Arraylist names;
    Arraylist interfaces;
    char *name;
} EagleStructType;

typedef struct {
    EagleBasicType type;
    Arraylist names;
} EagleInterfaceType;

typedef struct {
    EagleBasicType type;
    char *name;
} EagleEnumType;

EagleComplexType *et_parse_string(char *text);
EagleBasicType et_promotion(EagleBasicType left, EagleBasicType right);
EagleBasicType et_eagle_type(LLVMTypeRef ty);

EagleComplexType *ett_base_type(EagleBasicType type);
EagleComplexType *ett_pointer_type(EagleComplexType *to);
EagleComplexType *ett_array_type(EagleComplexType *of, int ct);
EagleComplexType *ett_function_type(EagleComplexType *retVal, EagleComplexType **params, int pct);
EagleComplexType *ett_gen_type(EagleComplexType *ytype);
EagleComplexType *ett_struct_type(char *name);
EagleComplexType *ett_class_type(char *name);
EagleComplexType *ett_interface_type(char *name);
EagleComplexType *ett_enum_type(char *name);
void ett_composite_interface(EagleComplexType *ett, char *name);

LLVMTypeRef ett_closure_type(EagleComplexType *type);
EagleBasicType ett_get_base_type(EagleComplexType *type);
EagleComplexType *ett_get_root_pointee(EagleComplexType *type);
LLVMTypeRef ett_llvm_type(EagleComplexType *type);
LLVMValueRef ett_default_value(EagleComplexType *type);
int ett_are_same(EagleComplexType *left, EagleComplexType *right);
int ett_pointer_depth(EagleComplexType *t);
int ett_size_of_type(EagleComplexType *t);
int ett_array_has_counted(EagleComplexType *t);
int ett_array_count(EagleComplexType *t);
char *ett_unique_type_name(EagleComplexType *t);

LLVMTypeRef ty_class_indirect();

void ett_debug_print(EagleComplexType *t);

void ty_prepare();
void ty_add_name(char *name);
int ty_is_name(char *name);
int ty_is_class(char *name);
int ty_is_interface(char *name);
int ty_is_enum(char *name);
void ty_teardown();

void ty_register_interface(char *name);
void ty_register_class(char *name);
void ty_register_typedef(char *name);
void ty_register_enum(char *name);
void ty_add_init(char *name, EagleComplexType *ty);
EagleComplexType *ty_get_init(char *name);
void ty_add_method(char *name, char *method, EagleComplexType *ty);
EagleComplexType *ty_method_lookup(char *name, char *method);
void ty_add_struct_def(char *name, Arraylist *names, Arraylist *types);
void ty_struct_member_index(EagleComplexType *ett, char *member, int *index, EagleComplexType **type);
void ty_struct_get_members(EagleComplexType *ett, Arraylist **names, Arraylist **types);
int ty_needs_destructor(EagleComplexType *ett);
LLVMTypeRef ty_get_counted(LLVMTypeRef in);
void ty_set_typedef(char *name, EagleComplexType *type);
void ty_add_enum_item(char *name, char *item, long val);
long ty_lookup_enum_item(EagleComplexType *ty, char *item, int *valid);

void ty_add_interface_method(char *name, char *method, EagleComplexType *ty);
int ty_interface_offset(char *name, char *method);
int ty_interface_count(char *name);
char *ty_interface_for_method(EagleComplexType *ett, char *method);
int ty_class_implements_interface(EagleComplexType *type, EagleComplexType *interface);
void ett_class_set_interfaces(EagleComplexType *ett, Arraylist *interfaces);

static inline int ET_IS_INT(EagleBasicType t)
{
    switch(t)
    {
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
        case ETUInt8:
        case ETUInt16:
        case ETUInt32:
        case ETUInt64:
            return 1;
        default:
            return 0;
    }
}

static inline int ET_IS_LLVM_INT(EagleBasicType t)
{
    switch(t)
    {
        case ETInt1:
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
        case ETUInt8:
        case ETUInt16:
        case ETUInt32:
        case ETUInt64:
            return 1;
        default:
            return 0;
    }
}

static inline int ET_IS_REAL(EagleBasicType t)
{
    switch(t)
    {
        case ETFloat:
        case ETDouble:
            return 1;
        default:
            return 0;
    }
}

static inline int ett_is_numeric(EagleComplexType *t)
{
    EagleBasicType type = t->type;
    switch(type)
    {
        case ETInt1:
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
        case ETFloat:
        case ETDouble:
        case ETUInt8:
        case ETUInt16:
        case ETUInt32:
        case ETUInt64:
            return 1;
        default:
            return 0;
    }
}

#endif
