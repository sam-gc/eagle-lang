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
    ETInt32,
    ETDouble,
    ETVoid,
    ETFunction
} EagleType;

EagleType et_parse_string(char *text);
EagleType et_promotion(EagleType left, EagleType right);
EagleType et_eagle_type(LLVMTypeRef ty);
LLVMTypeRef et_llvm_type(EagleType type);

#endif /* defined(__Eagle__types__) */
