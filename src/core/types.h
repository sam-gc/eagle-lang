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

#define NO_CLOSURE 0
#define CLOSURE_NO_CLOSE 1
#define CLOSURE_CLOSE 2
#define CLOSURE_RECURSE 3

#define ET_IS_INT(e) ((e) == ETInt8 || (e) == ETInt32 || (e) == ETInt64)
#define ET_IS_REAL(e) ((e) == ETDouble)
#define ET_IS_GEN_ARR(e) (((EagleArrayType *)(e))->ct < 0)
#define ET_IS_COUNTED(p) ((p)->type == ETPointer && ((EaglePointerType *)(p))->counted)
#define ET_IS_WEAK(p) ((p)->type == ETPointer && ((EaglePointerType *)(p))->weak)
#define ET_IS_CLOSED(p) ((p)->type == ETPointer && ((EaglePointerType *)(p))->closed)
#define ET_IS_CLOSURE(p) ((p)->type == ETFunction && ((EagleFunctionType *)(p))->closure)
#define ET_HAS_CLOASED(p) ((p)->type == ETFunction && ((EagleFunctionType *)(p))->closure == CLOSURE_CLOSE)
#define ET_IS_RECURSE(p) ((p)->type == ETFunction && ((EagleFunctionType *)(p))->closure == CLOSURE_RECURSE)

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
    ETDouble,
    ETCString,
    ETPointer,
    ETArray,
    ETVoid,
    ETFunction,
    ETGenerator,
    ETStruct,
    ETClass,
} EagleType;

typedef struct {
    EagleType type;
} EagleTypeType;

typedef struct {
    EagleType type;
    EagleTypeType *to;
    int counted;
    int weak;
    int closed;
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

    int closure;
    int gen;
} EagleFunctionType;

typedef struct {
    EagleType type;
    EagleTypeType *ytype;
} EagleGenType;

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
EagleTypeType *ett_gen_type(EagleTypeType *ytype);
EagleTypeType *ett_struct_type(char *name);
EagleTypeType *ett_class_type(char *name);

LLVMTypeRef ett_closure_type(EagleTypeType *type);
EagleType ett_get_base_type(EagleTypeType *type);
LLVMTypeRef ett_llvm_type(EagleTypeType *type);
int ett_are_same(EagleTypeType *left, EagleTypeType *right);
int ett_pointer_depth(EagleTypeType *t);
int ett_is_numeric(EagleTypeType *t);
int ett_size_of_type(EagleTypeType *t);
int ett_array_has_counted(EagleTypeType *t);
int ett_array_count(EagleTypeType *t);

LLVMTypeRef ty_class_indirect();

void ett_debug_print(EagleTypeType *t);

void ty_prepare();
void ty_add_name(char *name);
int ty_is_name(char *name);
int ty_is_class(char *name);
void ty_teardown();

void ty_register_interface(char *name);
void ty_register_class(char *name);
void ty_add_init(char *name, EagleTypeType *ty);
EagleTypeType *ty_get_init(char *name);
void ty_add_method(char *name, char *method, EagleTypeType *ty);
EagleTypeType *ty_method_lookup(char *name, char *method);
void ty_add_struct_def(char *name, arraylist *names, arraylist *types);
void ty_struct_member_index(EagleTypeType *ett, char *member, int *index, EagleTypeType **type);
int ty_needs_destructor(EagleTypeType *ett);
LLVMTypeRef ty_get_counted(LLVMTypeRef in);

void ty_add_interface_method(char *name, char *method, EagleTypeType *ty);
int ty_interface_offset(char *name, char *method);
int ty_interface_count(char *name);

#endif /* defined(__Eagle__types__) */
