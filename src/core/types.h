//
//  types.h
//  Eagle
//
//  Created by Sam Olsen on 7/22/15.
//  Copyright (c) 2015 Sam Olsen. All rights reserved.
//

#ifndef __Eagle__types__
#define __Eagle__types__

#include <stdio.h>
#include "llvm_headers.h"
#include "arraylist.h"

#define ET_IS_INT(e) ((e) == ETInt8 || (e) == ETInt32 || (e) == ETInt64)
#define ET_IS_REAL(e) ((e) == ETDouble)
#define ET_IS_GEN_ARR(e) (((EagleArrayType *)(e))->ct < 0)
#define ET_IS_COUNTED(p) ((p)->type == ETPointer && ((EaglePointerType *)(p))->counted)

extern LLVMTargetDataRef etTargetData;
extern LLVMModuleRef the_module;

typedef enum {
    ETNone,
    ETAny,
    ETNil,
    ETInt1,
    ETInt8,
    ETInt32,
    ETInt64,
    ETDouble,
    ETPointer,
    ETArray,
    ETVoid,
    ETFunction,
    ETStruct
} EagleType;

typedef struct {
    EagleType type;
} EagleTypeType;

typedef struct {
    EagleType type;
    EagleTypeType *to;
    int counted;
} EaglePointerType;

typedef struct {
    EagleType type;
    EagleTypeType *of;
    int ct;
} EagleArrayType;

typedef struct {
    EagleType type;
    EagleTypeType *retType;
    EagleTypeType **params;
    int pct;
} EagleFunctionType;

typedef struct {
    EagleType type;
    arraylist types;
    arraylist names;
    char *name;
} EagleStructType;

EagleTypeType *et_parse_string(char *text);
EagleType et_promotion(EagleType left, EagleType right);
EagleType et_eagle_type(LLVMTypeRef ty);

EagleTypeType *ett_base_type(EagleType type);
EagleTypeType *ett_pointer_type(EagleTypeType *to);
EagleTypeType *ett_array_type(EagleTypeType *of, int ct);
EagleTypeType *ett_function_type(EagleTypeType *retVal, EagleTypeType **params, int pct);
EagleTypeType *ett_struct_type(char *name);

EagleType ett_get_base_type(EagleTypeType *type);
LLVMTypeRef ett_llvm_type(EagleTypeType *type);
int ett_are_same(EagleTypeType *left, EagleTypeType *right);
int ett_pointer_depth(EagleTypeType *t);
int ett_is_numeric(EagleTypeType *t);
int ett_size_of_type(EagleTypeType *t);
int ett_array_has_counted(EagleTypeType *t);
int ett_array_count(EagleTypeType *t);

void ett_debug_print(EagleTypeType *t);

void ty_prepare();
void ty_add_name(char *name);
int ty_is_name(char *name);
void ty_teardown();

void ty_add_struct_def(char *name, arraylist *names, arraylist *types);
void ty_struct_member_index(EagleTypeType *ett, char *member, int *index, EagleTypeType **type);

#endif /* defined(__Eagle__types__) */
