/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ast_compiler.h"

void ac_scope_leave_callback(LLVMValueRef pos, EagleComplexType *ty, void *data)
{
    CompilerBundle *cb = data;
    ac_decr_pointer(cb, &pos, ty);
}

void ac_scope_leave_array_callback(LLVMValueRef pos, EagleComplexType *ty, void *data)
{
    CompilerBundle *cb = data;
    ac_decr_in_array(cb, pos, ett_array_count(ty));
}

void ac_scope_leave_weak_callback(LLVMValueRef pos, EagleComplexType *ty, void *data)
{
    CompilerBundle *cb = data;
    ac_remove_weak_pointer(cb, pos, ty);
}

void ac_decr_loaded_transients(void *key, void *val, void *data)
{
    CompilerBundle *cb = data;
    AST *ast = key;
    LLVMValueRef pos = val;

    if(ast->resultantType->type == ETPointer)
        ac_decr_val_pointer(cb, &pos, ast->resultantType);
    else
        ac_call_destructor(cb, pos, ast->resultantType);
}

void ac_decr_transients(void *key, void *val, void *data)
{
    CompilerBundle *cb = data;
    AST *ast = key;
    LLVMValueRef pos = val;

    ac_check_pointer(cb, &pos, ast->resultantType);
}

void ac_unwrap_pointer(CompilerBundle *cb, LLVMValueRef *ptr, EagleComplexType *ty, int keepptr)
{
    if(ty)
    {
        EaglePointerType *pt = (EaglePointerType *)ty;
        if(!pt->counted && !pt->weak)
            return;
    }

    LLVMValueRef tptr = *ptr;//LLVMBuildLoad(cb->builder, *ptr, "tptr");

    LLVMValueRef pos = LLVMBuildStructGEP(cb->builder, tptr, 5, "unwrap");

    *ptr = pos;
}

void ac_incr_val_pointer(CompilerBundle *cb, LLVMValueRef *ptr, EagleComplexType *ty)
{
    LLVMBuilderRef builder = cb->builder;

    if(ty)
    {
        EaglePointerType *pt = (EaglePointerType *)ty;
        if(!pt->counted)
            return;
    }

    LLVMValueRef tptr = *ptr;
    tptr = LLVMBuildBitCast(builder, tptr, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "cast");

    LLVMValueRef func = LLVMGetNamedFunction(cb->module, "__egl_incr_ptr");
    LLVMBuildCall(builder, func, &tptr, 1, "");
}

void ac_incr_pointer(CompilerBundle *cb, LLVMValueRef *ptr, EagleComplexType *ty)
{
    LLVMBuilderRef builder = cb->builder;

    if(ty)
    {
        EaglePointerType *pt = (EaglePointerType *)ty;
        if(!pt->counted)
            return;
    }

    LLVMValueRef tptr = LLVMBuildLoad(builder, *ptr, "tptr");
    tptr = LLVMBuildBitCast(builder, tptr, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "cast");

    LLVMValueRef func = LLVMGetNamedFunction(cb->module, "__egl_incr_ptr");
    LLVMBuildCall(builder, func, &tptr, 1, "");
}

void ac_check_pointer(CompilerBundle *cb, LLVMValueRef *ptr, EagleComplexType *ty)
{
    EaglePointerType *pt = (EaglePointerType *)ty;
    if(!pt->counted)
        return;

    LLVMValueRef tptr = LLVMBuildBitCast(cb->builder, *ptr, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "");
    LLVMValueRef func = LLVMGetNamedFunction(cb->module, "__egl_check_ptr");
    LLVMBuildCall(cb->builder, func, &tptr, 1, "");
}

void ac_prepare_pointer(CompilerBundle *cb, LLVMValueRef ptr, EagleComplexType *ty)
{
    if(ty && !ET_IS_COUNTED(ty))
        return;

    LLVMValueRef tptr = LLVMBuildBitCast(cb->builder, ptr, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "");
    LLVMValueRef func = LLVMGetNamedFunction(cb->module, "__egl_prepare");
    LLVMBuildCall(cb->builder, func, &tptr, 1, "");
}

void ac_add_weak_pointer(CompilerBundle *cb, LLVMValueRef ptr, LLVMValueRef weak, EagleComplexType *ty)
{
    if(!ET_IS_WEAK(ty))
        return;

    LLVMValueRef vals[2];
    vals[0] = LLVMBuildBitCast(cb->builder, ptr, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "");
    vals[1] = LLVMBuildBitCast(cb->builder, weak, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "");
    LLVMValueRef func = LLVMGetNamedFunction(cb->module, "__egl_add_weak");
    LLVMBuildCall(cb->builder, func, vals, 2, "");
}

void ac_remove_weak_pointer(CompilerBundle *cb, LLVMValueRef weak, EagleComplexType *ty)
{
    if(!ET_IS_WEAK(ty))
        return;

    LLVMValueRef val = LLVMBuildBitCast(cb->builder, weak, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "");
    LLVMValueRef func = LLVMGetNamedFunction(cb->module, "__egl_remove_weak");
    LLVMBuildCall(cb->builder, func, &val, 1, "");
}

void ac_decr_val_pointer(CompilerBundle *cb, LLVMValueRef *ptr, EagleComplexType *ty)
{
    LLVMBuilderRef builder = cb->builder;
    EaglePointerType *pt = (EaglePointerType *)ty;
    if(pt)
    {
        if(!pt->counted)
            return;
    }

    LLVMValueRef tptr = *ptr;
    tptr = LLVMBuildBitCast(builder, tptr, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "cast");

    LLVMValueRef func = LLVMGetNamedFunction(cb->module, "__egl_decr_ptr");
    LLVMBuildCall(builder, func, &tptr, 1, "");
}

void ac_decr_val_pointer_no_free(CompilerBundle *cb, LLVMValueRef *ptr, EagleComplexType *ty)
{
    LLVMBuilderRef builder = cb->builder;
    EaglePointerType *pt = (EaglePointerType *)ty;
    if(pt)
    {
        if(!pt->counted)
            return;
    }

    LLVMValueRef tptr = *ptr;
    tptr = LLVMBuildBitCast(builder, tptr, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "cast");

    LLVMValueRef func = LLVMGetNamedFunction(cb->module, "__egl_decr_no_free");
    LLVMBuildCall(builder, func, &tptr, 1, "");
}

void ac_nil_fill_array(CompilerBundle *cb, LLVMValueRef arr, int ct)
{
    LLVMBuilderRef builder = cb->builder;
    arr = LLVMBuildBitCast(builder, arr, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "");

    LLVMValueRef func = LLVMGetNamedFunction(cb->module, "__egl_array_fill_nil");
    LLVMValueRef vals[] = {arr, LLVMConstInt(LLVMInt64TypeInContext(utl_get_current_context()), ct, 0)};
    LLVMBuildCall(builder, func, vals, 2, "");
}

void ac_decr_in_array(CompilerBundle *cb, LLVMValueRef arr, int ct)
{
    LLVMBuilderRef builder = cb->builder;
    arr = LLVMBuildBitCast(builder, arr, LLVMPointerType(LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), 0), "");

    LLVMValueRef func = LLVMGetNamedFunction(cb->module, "__egl_array_decr_ptrs");
    LLVMValueRef vals[] = {arr, LLVMConstInt(LLVMInt64TypeInContext(utl_get_current_context()), ct, 0)};
    LLVMBuildCall(builder, func, vals, 2, "");
}

void ac_decr_pointer(CompilerBundle *cb, LLVMValueRef *ptr, EagleComplexType *ty)
{
    LLVMBuilderRef builder = cb->builder;

    if(ty)
    {
        EaglePointerType *pt = (EaglePointerType *)ty;
        if(!pt->counted)
            return;
    }

    LLVMValueRef tptr = LLVMBuildLoad(builder, *ptr, "tptr");
    tptr = LLVMBuildBitCast(builder, tptr, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "cast");

    LLVMValueRef func = LLVMGetNamedFunction(cb->module, "__egl_decr_ptr");
    LLVMBuildCall(builder, func, &tptr, 1, "");
}
