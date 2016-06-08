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

LLVMValueRef ac_try_view_conversion(CompilerBundle *cb, LLVMValueRef val, EagleComplexType *from, EagleComplexType *to)
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

LLVMValueRef ac_build_conversion(CompilerBundle *cb, LLVMValueRef val, EagleComplexType *from, EagleComplexType *to, int try_view, int lineno)
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
                die(lineno, msgerr_invalid_ptr_conversion);

            if(ett_get_base_type(to) == ETAny)
                return LLVMBuildBitCast(builder, val, ett_llvm_type(to), "ptrtmp");
            if(ett_get_base_type(from) == ETAny && ett_pointer_depth(from) == 1)
                return LLVMBuildBitCast(builder, val, ett_llvm_type(to), "ptrtmp");

            if(ET_IS_COUNTED(to) != ET_IS_COUNTED(from) || ET_IS_WEAK(to) != ET_IS_WEAK(from))
                die(lineno, msgerr_invalid_counted_uncounted_conversion);

            if(ett_pointer_depth(to) != ett_pointer_depth(from))
                die(lineno, msgerr_mismatched_ptr_depth, ett_pointer_depth(to), ett_pointer_depth(from));

            if(ett_get_base_type(to) == ETInterface)
            {
                if(!ty_class_implements_interface(from, to))
                    die(lineno, msgerr_interface_not_impl_by_class);
            }
            else if(!ett_are_same(ett_get_root_pointee(to), ett_get_root_pointee(from)))
            {
                LLVMDumpType(ett_llvm_type(ett_get_root_pointee(from)));
                die(lineno, msgerr_invalid_implicit_ptr_conversion);
            }

            return LLVMBuildBitCast(builder, val, ett_llvm_type(to), "ptrtmp");
        }
        case ETArray:
        {
            if(to->type != ETPointer && to->type != ETArray)
                die(lineno, msgerr_invalid_array_conversion);

            //LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(utl_get_current_context()), 0, 0);
            return LLVMBuildBitCast(builder, val, ett_llvm_type(to), "ptrtmp");
        }
        case ETInt1:
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
        case ETUInt8:
        case ETUInt16:
        case ETUInt32:
        case ETUInt64:
            switch(to->type)
            {
                case ETInt1:
                    return LLVMBuildICmp(builder, LLVMIntNE, val, LLVMConstInt(ett_llvm_type(from), 0, 0), "cmp");
                case ETInt8:
                case ETInt16:
                case ETInt32:
                case ETInt64:
                case ETUInt8:
                case ETUInt16:
                case ETUInt32:
                case ETUInt64:
                    return LLVMBuildIntCast(builder, val, ett_llvm_type(to), "conv");
                case ETFloat:
                case ETDouble:
                    return LLVMBuildSIToFP(builder, val, ett_llvm_type(to), "conv");
                default:
                    die(lineno, msgerr_invalid_implicit_conversion);
                    break;
            }
        case ETFloat:
        case ETDouble:
            switch(to->type)
            {
                case ETFloat:
                case ETDouble:
                    return LLVMBuildFPCast(builder, val, ett_llvm_type(to), "conv");
                case ETInt1:
                    return LLVMBuildFCmp(builder, LLVMRealONE, val, LLVMConstReal(0, 0), "cmp");
                case ETInt8:
                case ETInt16:
                case ETInt32:
                case ETInt64:
                    return LLVMBuildFPToSI(builder, val, ett_llvm_type(to), "conv");
                case ETUInt8:
                case ETUInt16:
                case ETUInt32:
                case ETUInt64:
                    return LLVMBuildFPToSI(builder, val, ett_llvm_type(to), "conv");
                default:
                    die(lineno, msgerr_invalid_implicit_conversion);
                    break;
            }
            break;
        case ETCString:
            if(to->type == ETCString)
                return val;
            if(to->type == ETPointer && ((EaglePointerType *)to)->to->type == ETInt8)
                return val;
            die(lineno, msgerr_invalid_implicit_conversion);
            break;
        default:
            die(lineno, msgerr_invalid_implicit_conversion);
            break;
    }

    return NULL;
}

LLVMValueRef ac_make_add(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleBasicType type, int lineno)
{
    switch(type)
    {
        case ETFloat:
        case ETDouble:
            return LLVMBuildFAdd(builder, left, right, "addtmp");
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
        case ETUInt8:
        case ETUInt16:
        case ETUInt32:
        case ETUInt64:
            return LLVMBuildAdd(builder, left, right, "addtmp");
        default:
            die(lineno, msgerr_invalid_sum_types);
            return NULL;
    }
}

LLVMValueRef ac_make_sub(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleBasicType type, int lineno)
{
    switch(type)
    {
        case ETFloat:
        case ETDouble:
            return LLVMBuildFSub(builder, left, right, "subtmp");
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
        case ETUInt8:
        case ETUInt16:
        case ETUInt32:
        case ETUInt64:
            return LLVMBuildSub(builder, left, right, "subtmp");
        default:
            die(lineno, msgerr_invalid_sub_types);
            return NULL;
    }
}

LLVMValueRef ac_make_mul(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleBasicType type, int lineno)
{
    switch(type)
    {
        case ETFloat:
        case ETDouble:
            return LLVMBuildFMul(builder, left, right, "multmp");
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
        case ETUInt8:
        case ETUInt16:
        case ETUInt32:
        case ETUInt64:
            return LLVMBuildMul(builder, left, right, "multmp");
        default:
            die(lineno, msgerr_invalid_mul_types);
            return NULL;
    }
}

LLVMValueRef ac_make_div(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleBasicType type, int lineno)
{
    switch(type)
    {
        case ETFloat:
        case ETDouble:
            return LLVMBuildFDiv(builder, left, right, "divtmp");
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
            return LLVMBuildSDiv(builder, left, right, "divtmp");
        case ETUInt8:
        case ETUInt16:
        case ETUInt32:
        case ETUInt64:
            return LLVMBuildUDiv(builder, left, right, "divtmp");
        default:
            die(lineno, msgerr_invalid_div_types);
            return NULL;
    }
}

LLVMValueRef ac_make_mod(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleBasicType type, int lineno)
{
    switch(type)
    {
        case ETFloat:
        case ETDouble:
            return LLVMBuildFRem(builder, left, right, "modtmp");
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
            return LLVMBuildSRem(builder, left, right, "modtmp");
        case ETUInt8:
        case ETUInt16:
        case ETUInt32:
        case ETUInt64:
            return LLVMBuildURem(builder, left, right, "modtmp");
        default:
            die(lineno, msgerr_invalid_mod_types);
            return NULL;
    }
}

LLVMValueRef ac_make_bitor(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleBasicType type, int lineno)
{
    switch(type)
    {
        case ETFloat:
        case ETDouble:
            die(lineno, msgerr_invalid_OR_types);
            return NULL;
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
        case ETUInt8:
        case ETUInt16:
        case ETUInt32:
        case ETUInt64:
            return LLVMBuildOr(builder, left, right, "OR");
        default:
            die(lineno, msgerr_invalid_OR_types);
            return NULL;
    }
}

LLVMValueRef ac_make_bitand(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleBasicType type, int lineno)
{
    switch(type)
    {
        case ETFloat:
        case ETDouble:
            die(lineno, msgerr_invalid_AND_types);
            return NULL;
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
        case ETUInt8:
        case ETUInt16:
        case ETUInt32:
        case ETUInt64:
            return LLVMBuildAnd(builder, left, right, "AND");
        default:
            die(lineno, msgerr_invalid_AND_types);
            return NULL;
    }
}

LLVMValueRef ac_make_bitxor(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleBasicType type, int lineno)
{
    switch(type)
    {
        case ETFloat:
        case ETDouble:
            die(lineno, msgerr_invalid_XOR_types);
            return NULL;
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
        case ETUInt8:
        case ETUInt16:
        case ETUInt32:
        case ETUInt64:
            return LLVMBuildXor(builder, left, right, "XOR");
        default:
            die(lineno, msgerr_invalid_XOR_types);
            return NULL;
    }
}

LLVMValueRef ac_make_bitshift(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleBasicType type, int lineno, int dir)
{
    LLVMValueRef (*shift_func)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef, const char*);

    if(dir == LEFT)
        shift_func = &LLVMBuildShl;
    else
        shift_func = &LLVMBuildAShr;

    switch(type)
    {
        case ETFloat:
        case ETDouble:
            die(lineno, msgerr_invalid_shift_types);
            return NULL;
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
        case ETUInt8:
        case ETUInt16:
        case ETUInt32:
        case ETUInt64:
            return shift_func(builder, left, right, "shift");
        default:
            die(lineno, msgerr_invalid_shift_types);
            return NULL;
    }
}

LLVMValueRef ac_make_neg(LLVMValueRef val, LLVMBuilderRef builder, EagleBasicType type, int lineno)
{
    switch(type)
    {
        case ETFloat:
        case ETDouble:
            return LLVMBuildFNeg(builder, val, "negtmp");
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
        case ETUInt8:
        case ETUInt16:
        case ETUInt32:
        case ETUInt64:
            return LLVMBuildNeg(builder, val, "negtmp");
        default:
            die(lineno, msgerr_invalid_neg_type, type);
            return NULL;
    }
}

LLVMValueRef ac_make_bitnot(LLVMValueRef val, LLVMBuilderRef builder, EagleBasicType type, int lineno)
{
    switch(type)
    {
        case ETFloat:
        case ETDouble:
            die(lineno, msgerr_invalid_NOT_type);
            return NULL;
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
        case ETUInt8:
        case ETUInt16:
        case ETUInt32:
        case ETUInt64:
            return LLVMBuildNot(builder, val, "NOT");
        default:
            die(lineno, msgerr_invalid_NOT_type);
            return NULL;
    }
}

LLVMValueRef ac_make_comp(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleBasicType type, char comp, int lineno)
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
        case ETFloat:
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
            die(lineno, msgerr_invalid_comparison_types);
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

