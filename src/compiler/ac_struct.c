/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ast_compiler.h"
#include "core/utils.h"
#include "core/config.h"

void ac_scope_leave_struct_callback(LLVMValueRef pos, EagleComplexType *ty, void *data)
{
    CompilerBundle *cb = data;
    ac_call_destructor(cb, pos, ty);
}

LLVMValueRef ac_gen_struct_destructor_func(char *name, CompilerBundle *cb)
{
    char *buf = malloc(strlen(name) + 9);
    sprintf(buf, "__egl_x_%s", name);

    LLVMValueRef func = LLVMGetNamedFunction(cb->module, buf);
    if(func)
    {
        free(buf);
        return func;
    }

    LLVMTypeRef tys[2];
    tys[0] = LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0);
    tys[1] = LLVMInt1TypeInContext(utl_get_current_context());
    func = LLVMAddFunction(cb->module, buf, LLVMFunctionType(LLVMVoidTypeInContext(utl_get_current_context()), tys, 2, 0));

    free(buf);
    return func;
}

LLVMValueRef ac_gen_struct_constructor_func(char *name, CompilerBundle *cb, int copy)
{
    char *buf = malloc(strlen(name) + 9);
    copy ? sprintf(buf, "__egl_c_%s", name) : sprintf(buf, "__egl_i_%s", name);

    LLVMValueRef func = LLVMGetNamedFunction(cb->module, buf);
    if(func)
    {
        free(buf);
        return func;
    }

    LLVMTypeRef ty = LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0);
    func = LLVMAddFunction(cb->module, buf, LLVMFunctionType(LLVMVoidTypeInContext(utl_get_current_context()), &ty, 1, 0));

    free(buf);
    return func;
}

void ac_make_struct_copy_constructor(AST *ast, CompilerBundle *cb)
{
    ASTStructDecl *a = (ASTStructDecl *)ast;
    LLVMValueRef func = ac_gen_struct_constructor_func(a->name, cb, 1);

    if(a->ext)
        return;

    EagleComplexType *ett = ett_pointer_type(ett_struct_type(a->name));

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(utl_get_current_context(), func, "entry");
    LLVMPositionBuilderAtEnd(cb->builder, entry);
    LLVMValueRef in = LLVMGetParam(func, 0);
    LLVMValueRef strct = LLVMBuildBitCast(cb->builder, in, ett_llvm_type(ett), "");


    Arraylist *types = &a->types;
    int i;
    for(i = 0; i < types->count; i++)
    {
        EagleComplexType *t = arr_get(types, i);
        if(ET_IS_COUNTED(t))
        {
            LLVMValueRef gep = LLVMBuildStructGEP(cb->builder, strct, i, "");
            ac_incr_pointer(cb, &gep, t);
        }
        else if(ET_IS_WEAK(t))
        {
            LLVMValueRef gep = LLVMBuildStructGEP(cb->builder, strct, i, "");
            ac_add_weak_pointer(cb, LLVMBuildLoad(cb->builder, gep, ""), gep, t);
        }
        else if(t->type == ETStruct && ty_needs_destructor(t))
        {
            LLVMValueRef fc = ac_gen_struct_constructor_func(((EagleStructType *)t)->name, cb, 1);
            LLVMValueRef param = LLVMBuildStructGEP(cb->builder, strct, i, "");
            param = LLVMBuildBitCast(cb->builder, param, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "");
            LLVMBuildCall(cb->builder, fc, &param, 1, "");
        }
    }

    LLVMBuildRetVoid(cb->builder);
}

void ac_make_struct_constructor(AST *ast, CompilerBundle *cb)
{
    ASTStructDecl *a = (ASTStructDecl *)ast;
    LLVMValueRef func = ac_gen_struct_constructor_func(a->name, cb, 0);

    if(a->ext)
        return;

    EagleComplexType *ett = ett_pointer_type(ett_struct_type(a->name));

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(utl_get_current_context(), func, "entry");
    LLVMPositionBuilderAtEnd(cb->builder, entry);
    LLVMValueRef in = LLVMGetParam(func, 0);
    LLVMValueRef strct = LLVMBuildBitCast(cb->builder, in, ett_llvm_type(ett), "");

    Arraylist *types = &a->types;
    int i;
    for(i = 0; i < types->count; i++)
    {
        EagleComplexType *t = arr_get(types, i);
        if(ET_IS_COUNTED(t) || ET_IS_WEAK(t))
        {
            LLVMValueRef gep = LLVMBuildStructGEP(cb->builder, strct, i, "");
            LLVMBuildStore(cb->builder, LLVMConstPointerNull(ett_llvm_type(t)), gep);
        }
        else if(t->type == ETStruct && ty_needs_destructor(t))
        {
            LLVMValueRef fc = ac_gen_struct_constructor_func(((EagleStructType *)t)->name, cb, 0);
            LLVMValueRef param = LLVMBuildStructGEP(cb->builder, strct, i, "");
            param = LLVMBuildBitCast(cb->builder, param, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "");
            LLVMBuildCall(cb->builder, fc, &param, 1, "");
        }
        else if(t->type == ETArray && ett_array_has_counted(t))
        {
            LLVMValueRef gep = LLVMBuildStructGEP(cb->builder, strct, i, "");
            ac_nil_fill_array(cb, gep, ett_array_count(t));
        }
    }

    LLVMBuildRetVoid(cb->builder);
}

void ac_make_struct_destructor(AST *ast, CompilerBundle *cb)
{
    ASTStructDecl *a = (ASTStructDecl *)ast;
    LLVMValueRef func = ac_gen_struct_destructor_func(a->name, cb);

    if(a->ext)
        return;

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(utl_get_current_context(), func, "entry");
    LLVMPositionBuilderAtEnd(cb->builder, entry);

    EaglePointerType *ett = (EaglePointerType *)ett_pointer_type(ett_struct_type(a->name));
    LLVMValueRef pos = LLVMBuildAlloca(cb->builder, ett_llvm_type((EagleComplexType *)ett), "");
    LLVMValueRef cmp = LLVMBuildICmp(cb->builder, LLVMIntEQ, LLVMGetParam(func, 1), LLVMConstInt(LLVMInt1TypeInContext(utl_get_current_context()), 1, 0), "");
    LLVMBasicBlockRef ifBB = LLVMAppendBasicBlockInContext(utl_get_current_context(), func, "if");
    LLVMBasicBlockRef elseBB = LLVMAppendBasicBlockInContext(utl_get_current_context(), func, "else");
    LLVMBasicBlockRef mergeBB = LLVMAppendBasicBlockInContext(utl_get_current_context(), func, "merge");
    LLVMBuildCondBr(cb->builder, cmp, ifBB, elseBB);
    LLVMPositionBuilderAtEnd(cb->builder, ifBB);

    ett->counted = 1;
    LLVMValueRef cast = LLVMBuildBitCast(cb->builder, LLVMGetParam(func, 0), ett_llvm_type((EagleComplexType *)ett), "");
    LLVMBuildStore(cb->builder, LLVMBuildStructGEP(cb->builder, cast, 5, ""), pos);
    LLVMBuildBr(cb->builder, mergeBB);

    ett->counted = 0;
    LLVMPositionBuilderAtEnd(cb->builder, elseBB);
    cast = LLVMBuildBitCast(cb->builder, LLVMGetParam(func, 0), ett_llvm_type((EagleComplexType *)ett), "");
    LLVMBuildStore(cb->builder, cast, pos);
    LLVMBuildBr(cb->builder, mergeBB);

    LLVMPositionBuilderAtEnd(cb->builder, mergeBB);
    pos = LLVMBuildLoad(cb->builder, pos, "");

    Arraylist *types = &a->types;
    int i;
    for(i = 0; i < types->count; i++)
    {
        EagleComplexType *t = arr_get(types, i);
        if(ET_IS_COUNTED(t))
        {
            LLVMValueRef gep = LLVMBuildStructGEP(cb->builder, pos, i, "");
            ac_decr_pointer(cb, &gep, t);
        }
        else if(ET_IS_WEAK(t))
        {
            LLVMValueRef gep = LLVMBuildStructGEP(cb->builder, pos, i, "");
            ac_remove_weak_pointer(cb, gep, t);
        }
        else if(t->type == ETStruct && ty_needs_destructor(t))
        {
            LLVMValueRef fc = ac_gen_struct_destructor_func(((EagleStructType *)t)->name, cb);
            LLVMValueRef params[2];
            params[0] = LLVMBuildBitCast(cb->builder, LLVMBuildStructGEP(cb->builder, pos, i, ""),
                    LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "");
            params[1] = LLVMConstInt(LLVMInt1TypeInContext(utl_get_current_context()), 0, 0);
            LLVMBuildCall(cb->builder, fc, params, 2, "");
        }
        else if(t->type == ETArray && ett_array_has_counted(t))
        {
            LLVMValueRef gep = LLVMBuildStructGEP(cb->builder, pos, i, "");
            ac_decr_in_array(cb, gep, ett_array_count(t));
        }
    }

    LLVMBuildRetVoid(cb->builder);
}

void ac_add_struct_declaration(AST *ast, CompilerBundle *cb)
{
    ASTStructDecl *a = (ASTStructDecl *)ast;
    LLVMStructCreateNamed(utl_get_current_context(), a->name);
}

void ac_make_struct_definitions(AST *ast, CompilerBundle *cb)
{
    AST *old = ast;
    for(; ast; ast = ast->next)
    {
        if(ast->type != ASTRUCTDECL)
            continue;

        ASTStructDecl *a = (ASTStructDecl *)ast;

        LLVMTypeRef *tys = malloc(sizeof(LLVMTypeRef) * a->types.count);
        int i;
        for(i = 0; i < a->types.count; i++)
            tys[i] = ett_llvm_type(arr_get(&a->types, i));

        LLVMTypeRef loaded = LLVMGetTypeByName(cb->module, a->name);

        if(!LLVMIsOpaqueStruct(loaded))
        {
            warn(ALN, msgwarn_struct_redeclaration, a->name);
            free(tys);
            continue;
        }
        LLVMStructSetBody(loaded, tys, a->types.count, 0);
        free(tys);

        ty_add_struct_def(a->name, &a->names, &a->types);
    }

    ast = old;
    for(; ast; ast = ast->next)
    {
        if(ast->type != ASTRUCTDECL)
            continue;

        ASTStructDecl *a = (ASTStructDecl *)ast;
        if(ty_needs_destructor(ett_struct_type(a->name)))
        {
            ac_make_struct_destructor(ast, cb);
            ac_make_struct_constructor(ast, cb);
            ac_make_struct_copy_constructor(ast, cb);
        }
    }
}

void ac_call_copy_constructor(CompilerBundle *cb, LLVMValueRef pos, EagleComplexType *ty)
{
    if(ET_IS_WEAK(ty) || ET_IS_COUNTED(ty))
        pos = LLVMBuildStructGEP(cb->builder, pos, 5, "");

    EagleStructType *st = ty->type == ETPointer ? (EagleStructType *)((EaglePointerType *)ty)->to
                                                : (EagleStructType *)ty;

    pos = LLVMBuildBitCast(cb->builder, pos, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "");

    LLVMValueRef func = ac_gen_struct_constructor_func(st->name, cb, 1);
    LLVMBuildCall(cb->builder, func, &pos, 1, "");
}

void ac_call_constructor(CompilerBundle *cb, LLVMValueRef pos, EagleComplexType *ty)
{
    if(ET_IS_WEAK(ty) || ET_IS_COUNTED(ty))
        pos = LLVMBuildStructGEP(cb->builder, pos, 5, "");

    EagleStructType *st = ty->type == ETPointer ? (EagleStructType *)((EaglePointerType *)ty)->to
                                                : (EagleStructType *)ty;

    pos = LLVMBuildBitCast(cb->builder, pos, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "");

    LLVMValueRef func = ac_gen_struct_constructor_func(st->name, cb, 0);
    LLVMBuildCall(cb->builder, func, &pos, 1, "");
}

void ac_call_destructor(CompilerBundle *cb, LLVMValueRef pos, EagleComplexType *ty)
{
    if(ET_IS_WEAK(ty) || ET_IS_COUNTED(ty))
        die(-1, msgerr_internal);

    EagleStructType *st = ty->type == ETPointer ? (EagleStructType *)((EaglePointerType *)ty)->to
                                                : (EagleStructType *)ty;
    pos = LLVMBuildBitCast(cb->builder, pos, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "");

    LLVMValueRef func = ac_gen_struct_destructor_func(st->name, cb);
    LLVMValueRef params[2];
    params[0] = pos;
    params[1] = LLVMConstInt(LLVMInt1TypeInContext(utl_get_current_context()), 0, 0);
    LLVMBuildCall(cb->builder, func, params, 2, "");
}

typedef struct {
    CompilerBundle *cb;
    ASTStructLit *a;
    EagleStructType *st;
    LLVMValueRef strct;
    Hashtable *set;
} LiteralHelper;

static void ac_struct_lit_each(void *key, void *val, void *data)
{
    LiteralHelper *lh = data;
    
    // Unpack the data
    CompilerBundle *cb = lh->cb;
    ASTStructLit *a    = lh->a;
    EagleStructType *st  = lh->st;
    LLVMValueRef strct = lh->strct;

    char *member = key;
    AST *exp = val;

    int index;
    EagleComplexType *memtype;
    EagleComplexType *fromtype;
    ty_struct_member_index((EagleComplexType *)st, member, &index, &memtype);

    if(index < 0)
        die(a->lineno, msgerr_member_not_in_struct, st->name, member);

    hst_put(lh->set, member, (void *)1, NULL, NULL);

    LLVMValueRef pos = LLVMBuildStructGEP(cb->builder, strct, index, "");
    LLVMValueRef value = NULL;
    
    // Enable nested structure literals
    if(exp->type == ASTRUCTLIT)
    {
        if(memtype->type != ETStruct)
            die(exp->lineno, msgerr_assign_struct_lit_member, member);

        ASTStructLit *asl = (ASTStructLit *)exp;
        if(!asl->name)
            asl->name = ((EagleStructType *)memtype)->name;
        value = ac_compile_struct_lit(exp, cb, pos);
    }
    else
    {
        if(memtype->type == ETEnum)
            cb->enum_lookup = memtype;

        value = ac_dispatch_expression(exp, cb);

        cb->enum_lookup = NULL;
    }

    fromtype = exp->resultantType;

    if(!ett_are_same(fromtype, memtype))
        value = ac_build_conversion(cb, value, fromtype, memtype, LOOSE_CONVERSION, exp->lineno);

    // We need to make sure to load the other structure if necessary
    if(fromtype->type == ETStruct && LLVMTypeOf(value) == ett_llvm_type(ett_pointer_type(fromtype)))
        value = LLVMBuildLoad(cb->builder, value, "");

    // Use utility function to ensure counted pointers are handled properly
    ac_safe_store(exp, cb, pos, value, memtype, 0, exp->type != ASTRUCTLIT);
}

LLVMValueRef ac_compile_struct_lit(AST *ast, CompilerBundle *cb, LLVMValueRef strct)
{
    ASTStructLit *a = (ASTStructLit *)ast;

    if(!a->name)
        die(ALN, msgerr_struct_literal_type_unknown);

    EagleStructType *st = (EagleStructType *)ett_struct_type(a->name);

    Hashtable *dict = &a->exprs;
    Hashtable visible = hst_create();

    LiteralHelper lh = {
        .cb = cb,
        .st = st,
        .a = a,
        .strct = strct,
        .set = &visible
    };

    hst_for_each(dict, &ac_struct_lit_each, &lh);

    Arraylist *snames;
    Arraylist *stypes;

    // Set the members that are not set explicitly by the programmer
    ty_struct_get_members((EagleComplexType *)st, &snames, &stypes);

    for(int i = 0; i < stypes->count; i++)
    {
        if(hst_get(&visible, snames->items[i], NULL, NULL))
            continue;

        LLVMValueRef pos = LLVMBuildStructGEP(cb->builder, strct, i, snames->items[i]);
        LLVMValueRef val = ett_default_value(stypes->items[i]);

        ac_safe_store(NULL, cb, pos, val, stypes->items[i], 0, 1);
    }

    a->resultantType = (EagleComplexType *)st;

    hst_free(&visible);
    return strct;
}

