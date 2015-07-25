//
//  types.c
//  Eagle
//
//  Created by Sam Olsen on 7/22/15.
//  Copyright (c) 2015 Sam Olsen. All rights reserved.
//

#include <stdlib.h>
#include <string.h>
#include "types.h"

#define TTEST(t, targ, out) if(!strcmp(t, targ)) return out
#define ETEST(t, a, b) if(a == b) return t

EagleType et_parse_string(char *text)
{
    TTEST(text, "int", ETInt32);
    TTEST(text, "double", ETDouble);
    TTEST(text, "void", ETVoid);
    
    return ETNone;
}

EagleType et_promotion(EagleType left, EagleType right)
{
    if(left == ETNone || left == ETVoid || right == ETNone || right == ETVoid)
        return ETNone;

    return left > right ? left : right;
}

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
        default:
            return NULL;
    }
}

EagleType et_eagle_type(LLVMTypeRef ty)
{
    ETEST(ETDouble, ty, LLVMDoubleType());
    ETEST(ETInt32, ty, LLVMInt32Type());
    ETEST(ETVoid, ty, LLVMVoidType());

    return ETNone;
}

