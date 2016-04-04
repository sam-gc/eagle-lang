/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ast_compiler.h"
#include "cpp/cpp.h"

long ahhd(void *k, void *d)
{
    return (long)k;
}

int ahed(void *k, void *d)
{
    return k == d;
}

LLVMValueRef ac_make_floating_string(CompilerBundle *cb, const char *text, const char *name)
{
    LLVMTypeRef func_type = LLVMFunctionType(LLVMInt8TypeInContext(utl_get_current_context()), NULL, 0, 0);
    LLVMValueRef func = LLVMAddFunction(cb->module, "", func_type);
    LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(utl_get_current_context(), func, "");
    LLVMPositionBuilderAtEnd(cb->builder, bb);
    LLVMValueRef str = LLVMBuildGlobalStringPtr(cb->builder, text, name);
    EGLEraseFunction(func);

    return str;
}

LLVMValueRef ac_try_view_conversion(CompilerBundle *cb, LLVMValueRef val, EagleTypeType *from, EagleTypeType *to)
{
    EaglePointerType *pt = (EaglePointerType *)from;
    if(pt->to->type != ETStruct && pt->to->type != ETClass)
        return NULL;

    EagleStructType *st = (EagleStructType *)pt->to;
    if(!ty_is_class(st->name))
        return NULL;

    char *type_name = ett_unique_type_name(to);

    if(!ty_method_lookup(st->name, type_name))
    {
        free(type_name);
        return NULL;
    }

    char *method = ac_gen_method_name(st->name, type_name);

    LLVMValueRef func = LLVMGetNamedFunction(cb->module, method);
    LLVMValueRef call = LLVMBuildCall(cb->builder, func, &val, 1, "convertable");

    free(method);
    free(type_name);

    return call;
}

LLVMValueRef ac_build_conversion(CompilerBundle *cb, LLVMValueRef val, EagleTypeType *from, EagleTypeType *to, int try_view, int lineno)
{
    LLVMBuilderRef builder = cb->builder;
    switch(from->type)
    {
        case ETPointer:
        {
            if(try_view)
            {
                LLVMValueRef cv = ac_try_view_conversion(cb, val, from, to);
                if(cv)
                    return cv;
            }

            if(to->type != ETPointer)
                die(lineno, "Non-pointer type may not be converted to pointer type.");

            if(ett_get_base_type(to) == ETAny)
                return LLVMBuildBitCast(builder, val, ett_llvm_type(to), "ptrtmp");
            if(ett_get_base_type(from) == ETAny && ett_pointer_depth(from) == 1)
                return LLVMBuildBitCast(builder, val, ett_llvm_type(to), "ptrtmp");

            if(ET_IS_COUNTED(to) != ET_IS_COUNTED(from) || ET_IS_WEAK(to) != ET_IS_WEAK(from))
                die(lineno, "Counted pointer type may not be converted to counted pointer type. Use the \"unwrap\" keyword.");

            if(ett_pointer_depth(to) != ett_pointer_depth(from))
                die(lineno, "Implicit pointer conversion invalid. Cannot conver pointer of depth %d to depth %d.", ett_pointer_depth(to), ett_pointer_depth(from));

            if(ett_get_base_type(to) == ETInterface)
            {
                if(!ty_class_implements_interface(from, to))
                    die(lineno, "Class does not implement the requested interface");
            }
            else if(!ett_are_same(ett_get_root_pointee(to), ett_get_root_pointee(from)))
            {
                LLVMDumpType(ett_llvm_type(ett_get_root_pointee(from)));
                die(lineno, "Implicit pointer conversion invalid; pointer types are incompatible.");
            }

            return LLVMBuildBitCast(builder, val, ett_llvm_type(to), "ptrtmp");
        }
        case ETArray:
        {
            if(to->type != ETPointer && to->type != ETArray)
                die(lineno, "Arrays may only be converted to equivalent pointers.");

            //LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(utl_get_current_context()), 0, 0);
            return LLVMBuildBitCast(builder, val, ett_llvm_type(to), "ptrtmp");
        }
        case ETInt1:
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
            switch(to->type)
            {
                case ETInt1:
                    return LLVMBuildICmp(builder, LLVMIntNE, val, LLVMConstInt(ett_llvm_type(from), 0, 0), "cmp");
                case ETInt8:
                case ETInt16:
                case ETInt32:
                case ETInt64:
                    return LLVMBuildIntCast(builder, val, ett_llvm_type(to), "conv");
                case ETDouble:
                    return LLVMBuildSIToFP(builder, val, LLVMDoubleTypeInContext(utl_get_current_context()), "conv");
                default:
                    die(lineno, "Invalid implicit conversion.");
                    break;
            }
        case ETDouble:
            switch(to->type)
            {
                case ETDouble:
                    return val;
                case ETInt1:
                    return LLVMBuildFCmp(builder, LLVMRealONE, val, LLVMConstReal(0, 0), "cmp");
                case ETInt8:
                case ETInt16:
                case ETInt32:
                case ETInt64:
                    return LLVMBuildFPToSI(builder, val, ett_llvm_type(to), "conv");
                default:
                    die(lineno, "Invalid implicit conversion from double.");
                    break;
            }
            break;
        case ETCString:
            if(to->type == ETCString)
                return val;
            if(to->type == ETPointer && ((EaglePointerType *)to)->to->type == ETInt8)
                return val;
            die(lineno, "Invalid implicit conversion to C string.");
            break;
        default:
            die(lineno, "Invalid implicit conversion.");
            break;
    }

    return NULL;
}

LLVMValueRef ac_make_add(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type, int lineno)
{
    switch(type)
    {
        case ETDouble:
            return LLVMBuildFAdd(builder, left, right, "addtmp");
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
            return LLVMBuildAdd(builder, left, right, "addtmp");
        default:
            die(lineno, "The given types may not be summed.");
            return NULL;
    }
}

LLVMValueRef ac_make_sub(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type, int lineno)
{
    switch(type)
    {
        case ETDouble:
            return LLVMBuildFSub(builder, left, right, "subtmp");
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
            return LLVMBuildSub(builder, left, right, "subtmp");
        default:
            die(lineno, "The given types may not be subtracted.");
            return NULL;
    }
}

LLVMValueRef ac_make_mul(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type, int lineno)
{
    switch(type)
    {
        case ETDouble:
            return LLVMBuildFMul(builder, left, right, "multmp");
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
            return LLVMBuildMul(builder, left, right, "multmp");
        default:
            die(lineno, "The given types may not be multiplied.");
            return NULL;
    }
}

LLVMValueRef ac_make_div(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type, int lineno)
{
    switch(type)
    {
        case ETDouble:
            return LLVMBuildFDiv(builder, left, right, "divtmp");
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
            return LLVMBuildSDiv(builder, left, right, "divtmp");
        default:
            die(lineno, "The given types may not be divided.");
            return NULL;
    }
}

LLVMValueRef ac_make_mod(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type, int lineno)
{
    switch(type)
    {
        case ETDouble:
            return LLVMBuildFRem(builder, left, right, "modtmp");
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
            return LLVMBuildSRem(builder, left, right, "modtmp");
        default:
            die(lineno, "The given types may not have modulo applied.");
            return NULL;
    }
}

LLVMValueRef ac_make_bitor(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type, int lineno)
{
    switch(type)
    {
        case ETDouble:
            die(lineno, "Bitwise OR does not apply to floating point types");
            return NULL;
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
            return LLVMBuildOr(builder, left, right, "OR");
        default:
            die(lineno, "The given types may not be OR'd");
            return NULL;
    }
}

LLVMValueRef ac_make_neg(LLVMValueRef val, LLVMBuilderRef builder, EagleType type, int lineno)
{
    switch(type)
    {
        case ETDouble:
            return LLVMBuildFNeg(builder, val, "negtmp");
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
            return LLVMBuildNeg(builder, val, "negtmp");
        default:
            die(lineno, "The given type may not have negation applied (%d).", type);
            return NULL;
    }
}

LLVMValueRef ac_make_comp(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type, char comp, int lineno)
{
    LLVMIntPredicate ip;
    LLVMRealPredicate rp;

    switch(comp)
    {
        case 'e':
            ip = LLVMIntEQ;
            rp = LLVMRealOEQ;
            break;
        case 'n':
            ip = LLVMIntNE;
            rp = LLVMRealONE;
            break;
        case 'g':
            ip = LLVMIntSGT;
            rp = LLVMRealOGT;
            break;
        case 'l':
            ip = LLVMIntSLT;
            rp = LLVMRealOLT;
            break;
        case 'G':
            ip = LLVMIntSGE;
            rp = LLVMRealOGE;
            break;
        case 'L':
            ip = LLVMIntSLE;
            rp = LLVMRealOLE;
            break;
        default:
            ip = LLVMIntEQ;
            rp = LLVMRealOEQ;
            break;
    }

    switch(type)
    {
        case ETDouble:
            return LLVMBuildFCmp(builder, rp, left, right, "eqtmp");
        case ETInt1:
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
        case ETEnum:
            return LLVMBuildICmp(builder, ip, left, right, "eqtmp");
        default:
            die(lineno, "The given types may not be compared.");
            return NULL;
    }
}

void ac_replace_with_counted(CompilerBundle *cb, VarBundle *b)
{

    LLVMBasicBlockRef isb = LLVMGetInsertBlock(cb->builder);
    LLVMValueRef val = b->value;
    LLVMPositionBuilderBefore(cb->builder, val);

    LLVMValueRef mal = ac_compile_malloc_counted(b->type, &b->type, val, cb);
    LLVMValueRef pos = LLVMBuildAlloca(cb->builder, ett_llvm_type(b->type), "");
    LLVMBuildStore(cb->builder, mal, pos);
    ac_incr_pointer(cb, &pos, b->type);

    ((EaglePointerType *)b->type)->closed = 1;

    b->value = pos;
    b->scopeCallback = ac_scope_leave_callback;
    b->scopeData = cb;

    LLVMValueRef to = LLVMBuildStructGEP(cb->builder, mal, 5, "");
    LLVMReplaceAllUsesWith(val, to);
    LLVMInstructionEraseFromParent(val);

    LLVMPositionBuilderAtEnd(cb->builder, isb);
}
