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

typedef enum {
    ETNone,
    ETAny,
    ETInt8,
    ETInt32,
    ETInt64,
    ETDouble,
    ETPointer,
    ETVoid,
    ETFunction
} EagleType;

typedef struct {
    EagleType type;
} EagleTypeType;

typedef struct {
    EagleType type;
    EagleTypeType *to;
} EaglePointerType;

typedef struct {
    EagleType type;
    EagleTypeType *retType;
    EagleTypeType **params;
    int pct;
} EagleFunctionType;

EagleTypeType *et_parse_string(char *text);
EagleType et_promotion(EagleType left, EagleType right);
EagleType et_eagle_type(LLVMTypeRef ty);
EagleTypeType *ett_base_type(EagleType type);
EagleTypeType *ett_pointer_type(EagleTypeType *to);
EagleTypeType *ett_function_type(EagleTypeType *retVal, EagleTypeType **params, int pct);
EagleType ett_get_base_type(EagleTypeType *type);
LLVMTypeRef ett_llvm_type(EagleTypeType *type);
int ett_are_same(EagleTypeType *left, EagleTypeType *right);
int ett_pointer_depth(EagleTypeType *t);

#endif /* defined(__Eagle__types__) */
