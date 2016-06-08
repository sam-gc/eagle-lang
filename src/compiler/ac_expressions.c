/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ast_compiler.h"

LLVMValueRef ac_compile_value(AST *ast, CompilerBundle *cb)
{
    ASTValue *a = (ASTValue *)ast;
    switch(a->etype)
    {
        case ETInt1:
            a->resultantType = ett_base_type(ETInt1);
            return LLVMConstInt(LLVMInt1TypeInContext(utl_get_current_context()), a->value.i, 1);
        case ETInt8:
            a->resultantType = ett_base_type(ETInt8);
            return LLVMConstInt(LLVMInt8TypeInContext(utl_get_current_context()), a->value.i, 1);
        case ETInt32:
            a->resultantType = ett_base_type(ETInt32);
            return LLVMConstInt(LLVMInt32TypeInContext(utl_get_current_context()), a->value.i, 1);
        case ETFloat:
            a->resultantType = ett_base_type(ETFloat);
            return LLVMConstReal(LLVMFloatTypeInContext(utl_get_current_context()), a->value.d);
        case ETDouble:
            a->resultantType = ett_base_type(ETDouble);
            return LLVMConstReal(LLVMDoubleTypeInContext(utl_get_current_context()), a->value.d);
        case ETCString:
            a->resultantType = ett_pointer_type(ett_base_type(ETInt8));
            return LLVMBuildGlobalStringPtr(cb->builder, a->value.id, "str");
        case ETNil:
            a->resultantType = ett_pointer_type(ett_base_type(ETAny));
            return LLVMConstPointerNull(ett_llvm_type(a->resultantType));
        default:
            die(ALN, msgerr_unknown_value_type);
            return NULL;
    }
}

LLVMValueRef ac_lookup_enum(EagleComplexType *et, char *item)
{
    int valid;
    long enum_val = ty_lookup_enum_item(et, item, &valid);

    if(!valid)
        return NULL;

    return LLVMConstInt(LLVMInt64TypeInContext(utl_get_current_context()), enum_val, 0);
}

LLVMValueRef ac_compile_identifier(AST *ast, CompilerBundle *cb)
{
    ASTValue *a = (ASTValue *)ast;
    VarBundle *b = vs_get(cb->varScope, a->value.id);

    if(!b && cb->enum_lookup)
    {
        LLVMValueRef enl = ac_lookup_enum(cb->enum_lookup, a->value.id);
        if(!enl)
            die(ALN, msgerr_item_not_in_enum, a->value.id, ((EagleEnumType *)cb->enum_lookup)->name);
        a->resultantType = cb->enum_lookup;
        return enl;
    }

    if(!b) // We are dealing with a local variable
        die(ALN, msgerr_undeclared_identifier, a->value.id);
    if(b->type->type == ETAuto)
        die(ALN, msgerr_unknown_var_read, a->value.id);

    b->wasused = 1;

    if(b->type->type == ETFunction)
    {
        a->resultantType = b->type;
        return b->value;
    }

    if(ET_IS_CLOSED(b->type))
    {
        a->resultantType = ((EaglePointerType *)b->type)->to;
        LLVMValueRef pos = LLVMBuildStructGEP(cb->builder, LLVMBuildLoad(cb->builder, b->value, ""), 5, "");

        if(a->resultantType->type == ETArray || a->resultantType->type == ETStruct || a->resultantType->type == ETClass || a->resultantType->type == ETInterface)
            return pos;

        return LLVMBuildLoad(cb->builder, pos, "loadtmp");
    }

    a->resultantType = b->type;

    if(b->type->type == ETArray && !ET_IS_GEN_ARR(b->type))
        return b->value;

    if(b->type->type == ETStruct || b->type->type == ETClass || b->type->type == ETInterface)
        return b->value;
    /*
    if((b->type->type == ETPointer && ((EaglePointerType *)b->type)->to->type == ETStruct) && (!ET_IS_COUNTED(b->type) && !ET_IS_WEAK(b->type)))
        return b->value;
        */

    return LLVMBuildLoad(cb->builder, b->value, "loadtmp");
}

LLVMValueRef ac_compile_var_decl_ext(EagleComplexType *type, char *ident, CompilerBundle *cb, int noSetNil, int lineno)
{
    LLVMBasicBlockRef curblock = LLVMGetInsertBlock(cb->builder);
    LLVMPositionBuilderAtEnd(cb->builder, cb->currentFunctionEntry);

    LLVMValueRef begin = LLVMGetFirstInstruction(cb->currentFunctionEntry);
    if(begin)
        LLVMPositionBuilderBefore(cb->builder, begin);
    LLVMValueRef pos = LLVMBuildAlloca(cb->builder, ett_llvm_type(type), ident);

    LLVMPositionBuilderAtEnd(cb->builder, curblock);

    VarBundle *b = vs_get(cb->varScope, ident);
    if(b && !b->value)
        b->value = pos;
    else if(b && vs_is_in_local_scope(cb->varScope, ident)) // A bundle exists and is already set
        die(lineno, msgerr_redeclaration, ident);

    if(ET_IS_COUNTED(type))
    {
        if(!noSetNil)
            LLVMBuildStore(cb->builder, LLVMConstPointerNull(ett_llvm_type(type)), pos);

        if(!cb->compilingMethod || strcmp(ident, "self"))
            vs_add_callback(cb->varScope, ident, ac_scope_leave_callback, cb);
    }
    else if(ET_IS_WEAK(type))
    {
        if(!noSetNil)
            LLVMBuildStore(cb->builder, LLVMConstPointerNull(ett_llvm_type(type)), pos);
        vs_add_callback(cb->varScope, ident, ac_scope_leave_weak_callback, cb);
    }
    else if(type->type == ETStruct && ty_needs_destructor(type))
    {
        ac_call_constructor(cb, pos, type);
        vs_add_callback(cb->varScope, ident, ac_scope_leave_struct_callback, cb);
    }

    if(type->type == ETArray && ett_array_has_counted(type))
    {
        ac_nil_fill_array(cb, pos, ett_array_count(type));
        vs_add_callback(cb->varScope, ident, ac_scope_leave_array_callback, cb);
    }

    return pos;
}

LLVMValueRef ac_compile_var_decl(AST *ast, CompilerBundle *cb)
{
    ASTVarDecl *a = (ASTVarDecl *)ast;
    ASTTypeDecl *type = (ASTTypeDecl *)a->atype;
    ast->resultantType = type->etype;

    if(vs_is_in_local_scope(cb->varScope, a->ident))
        die(ALN, msgerr_redefinition, a->ident);

    if(type->etype->type == ETAuto)
    {
        vs_put(cb->varScope, a->ident, NULL, type->etype, ALN);
        return NULL;
    }

    if(vl_is_static(a->linkage))
    {
        EagleComplexType *et = type->etype;

        LLVMValueRef glob = LLVMAddGlobal(cb->module, ett_llvm_type(et), a->ident); 
        LLVMValueRef init = ett_default_value(et);

        if(!init)
            die(ALN, msgerr_invalid_global_type);
        LLVMSetInitializer(glob, init);

        vs_put(cb->varScope, a->ident, glob, et, ALN);
        return glob;
    }

    vs_put(cb->varScope, a->ident, NULL, type->etype, ALN);
    LLVMValueRef pos = ac_compile_var_decl_ext(type->etype, a->ident, cb, a->noSetNil, ALN);

    return pos;
}

LLVMValueRef ac_compile_struct_member(AST *ast, CompilerBundle *cb, int keepPointer)
{
    ASTStructMemberGet *a = (ASTStructMemberGet *)ast;
    LLVMValueRef left = ac_dispatch_expression(a->left, cb);


    EagleComplexType *ty = a->left->resultantType;

    if(ty->type != ETPointer && ty->type != ETStruct && ty->type != ETClass && ty->type != ETInterface)
        die(ALN, msgerr_member_access_non_struct, a->ident);
    if(ty->type == ETPointer && ((EaglePointerType *)ty)->to->type != ETStruct && ((EaglePointerType *)ty)->to->type != ETClass &&
       ((EaglePointerType *)ty)->to->type != ETInterface)
        die(ALN, msgerr_member_access_non_struct_ptr, a->ident);

    LLVMValueRef lcw = a->left->type == AUNARY ? ((ASTUnary *)a->left)->savedWrapped : left;

    // Code to automatically dereference pointers-----------------------
    if(ty->type == ETPointer)
    {
        lcw = left;
        ac_unwrap_pointer(cb, &left, ty, 0);

        EaglePointerType *pt = (EaglePointerType *)ty;
        ty = pt->to;
    }
    // -----------------------------------------------------------------

    if(ty->type == ETInterface)
    {
        char *interface = ty_interface_for_method(ty, a->ident);

        a->leftCompiled = lcw; // a->left->type == AUNARY ? ((ASTUnary *)a->left)->savedWrapped : left;
        a->leftCompiled = LLVMBuildBitCast(cb->builder, a->leftCompiled, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "");
        EagleComplexType *ut = ty_method_lookup(interface, a->ident);
        LLVMValueRef func = LLVMGetNamedFunction(cb->module, "__egl_lookup_method");

        LLVMValueRef params[] = {left, //LLVMBuildStructGEP(cb->builder, left, 0, "vtable"),
                                 LLVMBuildGlobalStringPtr(cb->builder, interface, "ifc"),
                                 LLVMConstInt(LLVMInt32TypeInContext(utl_get_current_context()), ty_interface_offset(interface, a->ident), 0)
        };
        LLVMValueRef fptr = LLVMBuildCall(cb->builder, func, params, 3, a->ident);
        fptr = LLVMBuildBitCast(cb->builder, fptr, LLVMPointerType(ett_llvm_type(ut), 0), "");

        a->resultantType = ett_pointer_type(ut);
        return fptr; //LLVMBuildLoad(cb->builder, fptr, "");
    }

    // Only save the value of the instance if we have a class and a method.
    EagleComplexType *functionType = NULL;
    if((ty->type == ETClass || ty->type == ETStruct) && (functionType = ty_method_lookup(((EagleStructType *)ty)->name, a->ident)))
    {
        a->leftCompiled = lcw; // a->left->type == AUNARY ? ((ASTUnary *)a->left)->savedWrapped : left;
        char *name = ac_gen_method_name(((EagleStructType *)ty)->name, a->ident);
        LLVMValueRef func = LLVMGetNamedFunction(cb->module, name);
        if(!func) die(ALN, msgerr_internal_method_resolution);
        free(name);
        ast->resultantType = functionType;
        return func;
    }
        //a->leftCompiled = a->left->type == AUNARY ? ((ASTUnary *)a->left)->savedWrapped : left;

    int index;
    EagleComplexType *type;
    ty_struct_member_index(ty, a->ident, &index, &type);

    if(index < -1)
        die(ALN, msgerr_internal_struct_load_find);
    if(index < 0)
        die(ALN, msgerr_member_not_in_struct, ((EagleStructType *)ty)->name, a->ident);

    ast->resultantType = type;

    LLVMValueRef gep = LLVMBuildStructGEP(cb->builder, left, index, a->ident);
    if(keepPointer || type->type == ETStruct || type->type == ETClass || type->type == ETArray)
        return gep;
    return LLVMBuildLoad(cb->builder, gep, "");
}

LLVMValueRef ac_compile_type_lookup(AST *ast, CompilerBundle *cb)
{
    ASTTypeLookup *a = (ASTTypeLookup *)ast;

    if(!ty_is_enum(a->name))
        die(ALN, msgerr_invalid_type_lookup_type, a->name);

    EagleComplexType *et = ett_enum_type(a->name);
    int valid;
    long enum_val = ty_lookup_enum_item(et, a->item, &valid);

    if(!valid)
        die(ALN, msgerr_item_not_in_enum, a->name, a->item);

    a->resultantType = et;

    return LLVMConstInt(LLVMInt64TypeInContext(utl_get_current_context()), enum_val, 0);
}

LLVMValueRef ac_compile_malloc_counted_raw(LLVMTypeRef rt, LLVMTypeRef *out, CompilerBundle *cb)
{
    LLVMTypeRef ptmp[2];
    ptmp[0] = LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0);
    ptmp[1] = LLVMInt1TypeInContext(utl_get_current_context());

    LLVMTypeRef tys[6];
    tys[0] = LLVMInt64TypeInContext(utl_get_current_context());
    tys[1] = LLVMInt16TypeInContext(utl_get_current_context());
    tys[2] = LLVMInt16TypeInContext(utl_get_current_context());
    tys[3] = LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0);
    tys[4] = LLVMPointerType(LLVMFunctionType(LLVMVoidTypeInContext(utl_get_current_context()), ptmp, 2, 0), 0);
    tys[5] = rt;
    LLVMTypeRef tt = LLVMStructTypeInContext(utl_get_current_context(), tys, 6, 0);
    tt = ty_get_counted(tt);

    LLVMValueRef mal = LLVMBuildMalloc(cb->builder, tt, "new");

    *out = tt;

    ac_prepare_pointer(cb, mal, NULL);

    return mal;
}

LLVMValueRef ac_compile_malloc_counted(EagleComplexType *type, EagleComplexType **res, LLVMValueRef ib, CompilerBundle *cb)
{
    LLVMTypeRef ptmp[2];
    ptmp[0] = LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0);
    ptmp[1] = LLVMInt1TypeInContext(utl_get_current_context());

    LLVMTypeRef tys[6];
    tys[0] = LLVMInt64TypeInContext(utl_get_current_context());
    tys[1] = LLVMInt16TypeInContext(utl_get_current_context());
    tys[2] = LLVMInt16TypeInContext(utl_get_current_context());
    tys[3] = LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0);
    tys[4] = LLVMPointerType(LLVMFunctionType(LLVMVoidTypeInContext(utl_get_current_context()), ptmp, 2, 0), 0);
    tys[5] = ett_llvm_type(type);
    LLVMTypeRef tt = LLVMStructTypeInContext(utl_get_current_context(), tys, 6, 0);

    //LLVMDumpType(ett_llvm_type(type));
    tt = ty_get_counted(tt);

    //LLVMDumpType(tt);
    LLVMValueRef mal;
    if(ib)
        mal = EGLBuildMalloc(cb->builder, tt, ib, "new");
    else
        mal = LLVMBuildMalloc(cb->builder, tt, "new");

    EaglePointerType *pt = (EaglePointerType *)ett_pointer_type(type);
    pt->counted = 1;
    EagleComplexType *resultantType = (EagleComplexType *)pt;
    if(res)
        *res = resultantType;

    ac_prepare_pointer(cb, mal, resultantType);
    if((type->type == ETStruct && ty_needs_destructor(type)) || type->type == ETClass)
    {
        LLVMValueRef pos = LLVMBuildStructGEP(cb->builder, mal, 5, "");
        ac_call_constructor(cb, pos, type);
        pos = LLVMBuildStructGEP(cb->builder, mal, 4, "");
        EagleStructType *st = (EagleStructType *)type;
        LLVMBuildStore(cb->builder, ac_gen_struct_destructor_func(st->name, cb), pos);
    }

    // We need to specially handle counted counted types
    if(ET_IS_COUNTED(type))
    {
        LLVMValueRef pos = LLVMBuildStructGEP(cb->builder, mal, 4, "");
        LLVMBuildStore(cb->builder, LLVMGetNamedFunction(cb->module, "__egl_counted_destructor"), pos);

        pos = LLVMBuildStructGEP(cb->builder, mal, 5, "");
        LLVMBuildStore(cb->builder, LLVMConstPointerNull(ett_llvm_type(type)), pos);
    }
    /*
    LLVMValueRef pos = LLVMBuildStructGEP(cb->builder, mal, 0, "ctp");
    LLVMBuildStore(cb->builder, LLVMConstInt(LLVMInt64TypeInContext(utl_get_current_context()), 0, 0), pos);
    */


    return mal;
}

LLVMValueRef ac_compile_new_decl(AST *ast, CompilerBundle *cb)
{
    ASTBinary *a = (ASTBinary *)ast;
    ASTTypeDecl *type = (ASTTypeDecl *)a->left;

    LLVMValueRef val = ac_compile_malloc_counted(type->etype, &ast->resultantType, NULL, cb);
    hst_put(&cb->transients, ast, val, ahhd, ahed);

    EagleComplexType *to = ((EaglePointerType *)ast->resultantType)->to;
    if(a->right && to->type != ETClass)
    {
        // Do we have a structure literal?
        if(a->right->type == ASTRUCTLIT)
        {
            if(to->type != ETStruct)
                die(ALN, msgerr_invalid_struct_lit_assignment);

            ASTStructLit *asl = (ASTStructLit *)a->right;
            if(!asl->name)
                asl->name = ((EagleStructType *)to)->name;

            LLVMValueRef strct = LLVMBuildStructGEP(cb->builder, val, 5, "");
            ac_compile_struct_lit(a->right, cb, strct);
            return val;
        }

        LLVMValueRef init = ac_dispatch_expression(a->right, cb);
        
        if(!ett_are_same(a->right->resultantType, type->etype))
            init = ac_build_conversion(cb, init, a->right->resultantType, type->etype, LOOSE_CONVERSION, a->right->lineno);
        LLVMValueRef pos = LLVMBuildStructGEP(cb->builder, val, 5, "");
        LLVMBuildStore(cb->builder, init, pos);
    }
    else if(to->type == ETClass && ty_get_init(((EagleStructType *)to)->name))
    {
        // We have an init function defined...

        EagleStructType *st = (EagleStructType *)to;
        char *method_name = ac_gen_method_name(st->name, (char *)"__init__");
        LLVMValueRef func = LLVMGetNamedFunction(cb->module, method_name);
        free(method_name);

        EagleFunctionType *ett = (EagleFunctionType *)ty_get_init(st->name);

        AST *p;
        int i;
        for(p = a->right, i = 1; p; p = p->next, i++);
        int ct = i;

        LLVMValueRef args[ct];
        for(p = a->right, i = 1; p; p = p->next, i++)
        {
            LLVMValueRef val = ac_dispatch_expression(p, cb);
            EagleComplexType *rt = p->resultantType;

            if(i < ett->pct)
            {
                if(!ett_are_same(rt, ett->params[i]))
                    val = ac_build_conversion(cb, val, rt, ett->params[i], LOOSE_CONVERSION, p->lineno);
            }

            hst_remove_key(&cb->transients, p, ahhd, ahed);
            // hst_remove_key(&cb->loadedTransients, p, ahhd, ahed);
            args[i] = val;
        }

        args[0] = val;//LLVMBuildBitCast(cb->builder, , LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "");

        ac_incr_val_pointer(cb, &val, NULL);
        LLVMBuildCall(cb->builder, func, args, ct, "");
        ac_decr_val_pointer_no_free(cb, &val, NULL);
    }

    return val;
}

LLVMValueRef ac_compile_cast(AST *ast, CompilerBundle *cb)
{
    ASTCast *a = (ASTCast *)ast;
    ASTTypeDecl *ty = (ASTTypeDecl *)a->etype;

    LLVMValueRef val = ac_dispatch_expression(a->val, cb);

    EagleComplexType *to = ty->etype;
    EagleComplexType *from = a->val->resultantType;

    ast->resultantType = to;

    if(from->type == ETEnum)
        from = ett_base_type(ETInt64);
    if(to->type == ETEnum)
        to   = ett_base_type(ETInt64);

    if(ett_is_numeric(to) && ett_is_numeric(from))
    {
        return ac_build_conversion(cb, val, from, to, STRICT_CONVERSION, ALN);
    }

    if(to->type == ETPointer && from->type == ETPointer)
    {
        return LLVMBuildBitCast(cb->builder, val, ett_llvm_type(to), "casttmp");
    }

    if(to->type == ETPointer)
    {
        if(!ET_IS_INT(from->type))
            die(ALN, msgerr_non_int_ptr_cast);
        return LLVMBuildIntToPtr(cb->builder, val, ett_llvm_type(to), "casttmp");
    }

    if(from->type == ETPointer)
    {
        if(!ET_IS_INT(to->type))
            die(ALN, msgerr_ptr_cast);
        return LLVMBuildPtrToInt(cb->builder, val, ett_llvm_type(to), "casttmp");
    }

    die(ALN, msgerr_unknown_conversion);

    return NULL;
}

LLVMValueRef ac_compile_index(AST *ast, int keepPointer, CompilerBundle *cb)
{
    ASTBinary *a = (ASTBinary *)ast;
    AST *left = a->left;
    AST *right = a->right;

    LLVMValueRef l = ac_dispatch_expression(left, cb);
    LLVMValueRef r = ac_dispatch_expression(right, cb);

    EagleComplexType *lt = left->resultantType;
    EagleComplexType *rt = right->resultantType;

    if(lt->type != ETPointer && lt->type != ETArray)
        die(LN(left), msgerr_invalid_indexed_type);
    if(lt->type == ETPointer && ett_pointer_depth(lt) == 1 && ett_get_base_type(lt) == ETAny)
        die(LN(left), msgerr_deref_any_ptr);
    if(!ett_is_numeric(rt))
        die(LN(right), msgerr_invalid_indexer_type);

    if(ET_IS_REAL(rt->type))
        r = ac_build_conversion(cb, r, rt, ett_base_type(ETInt64), STRICT_CONVERSION, right->lineno);

    if(lt->type == ETPointer)
        ast->resultantType = ((EaglePointerType *)lt)->to;
    else
        ast->resultantType = ((EagleArrayType *)lt)->of;

    LLVMValueRef gep;
    if(lt->type == ETArray && ((EagleArrayType *)lt)->ct >= 0)
    {
        LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(utl_get_current_context()), 0, 0);
        LLVMValueRef pts[] = {zero, r};
        gep = LLVMBuildInBoundsGEP(cb->builder, l, pts, 2, "idx");
    }
    else
    {
        gep = LLVMBuildInBoundsGEP(cb->builder, l, &r, 1, "idx");
    }

    if(keepPointer || (lt->type == ETArray && ((EagleArrayType *)lt)->of->type == ETArray) || (lt->type == ETArray && ((EagleArrayType *)lt)->of->type == ETStruct))
        return gep;

    return LLVMBuildLoad(cb->builder, gep, "dereftmp");
}

// Short circuited logical or
LLVMValueRef ac_compile_logical_or(AST *ast, CompilerBundle *cb)
{
    ASTBinary *a = (ASTBinary *)ast;

    Arraylist trees = arr_create(10);
    Arraylist values = arr_create(10);
    Arraylist blocks = arr_create(10);

    for(; a->type == ABINARY && ((ASTBinary *)a)->op == '|'; a = (ASTBinary *)a->left)
        arr_append(&trees, a->right);

    arr_append(&trees, a);

    LLVMBasicBlockRef mergeBB = LLVMAppendBasicBlockInContext(utl_get_current_context(), cb->currentFunction, "merge");

    int i;
    for(i = trees.count - 1; i > 0; i--)
    {
        LLVMValueRef val = ac_dispatch_expression(trees.items[i], cb);
        LLVMValueRef cmp = ac_compile_test(trees.items[i], val, cb);

        LLVMBasicBlockRef nextBB = LLVMAppendBasicBlockInContext(utl_get_current_context(), cb->currentFunction, "next");

        arr_append(&values, LLVMConstInt(LLVMInt1TypeInContext(utl_get_current_context()), 1, 0));
        arr_append(&blocks, LLVMGetInsertBlock(cb->builder));

        hst_for_each(&cb->transients, ac_decr_transients, cb);
        hst_for_each(&cb->loadedTransients, ac_decr_loaded_transients, cb);

        hst_free(&cb->transients);
        hst_free(&cb->loadedTransients);

        cb->transients = hst_create();
        cb->loadedTransients = hst_create();

        LLVMBuildCondBr(cb->builder, cmp, mergeBB, nextBB);

        LLVMPositionBuilderAtEnd(cb->builder, nextBB);
    }

    LLVMValueRef val = ac_dispatch_expression(trees.items[0], cb);
    LLVMValueRef cmp = ac_compile_test(trees.items[0], val, cb);

    hst_for_each(&cb->transients, ac_decr_transients, cb);
    hst_for_each(&cb->loadedTransients, ac_decr_loaded_transients, cb);

    hst_free(&cb->transients);
    hst_free(&cb->loadedTransients);

    cb->transients = hst_create();
    cb->loadedTransients = hst_create();

    LLVMBuildBr(cb->builder, mergeBB);

    arr_append(&values, cmp);
    arr_append(&blocks, LLVMGetInsertBlock(cb->builder));

    LLVMMoveBasicBlockAfter(mergeBB, LLVMGetInsertBlock(cb->builder));
    LLVMPositionBuilderAtEnd(cb->builder, mergeBB);

    LLVMValueRef phi = LLVMBuildPhi(cb->builder, LLVMInt1TypeInContext(utl_get_current_context()), "phi");
    LLVMAddIncoming(phi, (LLVMValueRef *)values.items, (LLVMBasicBlockRef *)blocks.items, values.count);

    ast->resultantType = ett_base_type(ETInt1);

    arr_free(&trees);
    arr_free(&blocks);
    arr_free(&values);

    return phi;
}

// Short circuited logical and
LLVMValueRef ac_compile_logical_and(AST *ast, CompilerBundle *cb)
{
    ASTBinary *a = (ASTBinary *)ast;

    Arraylist trees = arr_create(10);
    Arraylist values = arr_create(10);
    Arraylist blocks = arr_create(10);

    for(; a->type == ABINARY && ((ASTBinary *)a)->op == '&'; a = (ASTBinary *)a->left)
        arr_append(&trees, a->right);

    arr_append(&trees, a);

    LLVMBasicBlockRef mergeBB = LLVMAppendBasicBlockInContext(utl_get_current_context(), cb->currentFunction, "merge");

    int i;
    for(i = trees.count - 1; i > 0; i--)
    {
        LLVMValueRef val = ac_dispatch_expression(trees.items[i], cb);
        LLVMValueRef cmp = ac_compile_test(trees.items[i], val, cb);

        // Any allocations made in the phi block must be destroyed there.
        // ==============================================================
        hst_for_each(&cb->transients, ac_decr_transients, cb);
        hst_for_each(&cb->loadedTransients, ac_decr_loaded_transients, cb);

        hst_free(&cb->transients);
        hst_free(&cb->loadedTransients);

        cb->transients = hst_create();
        cb->loadedTransients = hst_create();
        // ==============================================================

        LLVMBasicBlockRef nextBB = LLVMAppendBasicBlockInContext(utl_get_current_context(), cb->currentFunction, "next");
        LLVMBuildCondBr(cb->builder, cmp, nextBB, mergeBB);

        arr_append(&values, LLVMConstInt(LLVMInt1TypeInContext(utl_get_current_context()), 0, 0));
        arr_append(&blocks, LLVMGetInsertBlock(cb->builder));

        LLVMPositionBuilderAtEnd(cb->builder, nextBB);
    }

    LLVMValueRef val = ac_dispatch_expression(trees.items[0], cb);
    LLVMValueRef cmp = ac_compile_test(trees.items[0], val, cb);

    // ==============================================================
    hst_for_each(&cb->transients, ac_decr_transients, cb);
    hst_for_each(&cb->loadedTransients, ac_decr_loaded_transients, cb);

    hst_free(&cb->transients);
    hst_free(&cb->loadedTransients);

    cb->transients = hst_create();
    cb->loadedTransients = hst_create();
    // ==============================================================

    LLVMBuildBr(cb->builder, mergeBB);

    arr_append(&values, cmp);
    arr_append(&blocks, LLVMGetInsertBlock(cb->builder));

    LLVMMoveBasicBlockAfter(mergeBB, LLVMGetInsertBlock(cb->builder));
    LLVMPositionBuilderAtEnd(cb->builder, mergeBB);

    LLVMValueRef phi = LLVMBuildPhi(cb->builder, LLVMInt1TypeInContext(utl_get_current_context()), "phi");
    LLVMAddIncoming(phi, (LLVMValueRef *)values.items, (LLVMBasicBlockRef *)blocks.items, values.count);

    ast->resultantType = ett_base_type(ETInt1);

    arr_free(&trees);
    arr_free(&blocks);
    arr_free(&values);

    return phi;
}

LLVMValueRef ac_generic_unary(ASTUnary *a, LLVMValueRef val, CompilerBundle *cb)
{
    switch(a->op)
    {
        case '-':
            return ac_make_neg(val, cb->builder, a->val->resultantType->type, a->lineno);
        case '~':
            return ac_make_bitnot(val, cb->builder, a->val->resultantType->type, a->lineno);
        default:
            die(a->lineno, msgerr_internal);
    }

    return NULL;
}

LLVMValueRef
ac_generic_binary(ASTBinary *a, LLVMValueRef l, LLVMValueRef r,
                  char save_left, EagleComplexType *fromtype, EagleComplexType *totype, CompilerBundle *cb)
{
    if(
    (!save_left && (fromtype->type == ETPointer || totype->type == ETPointer)) ||
    (save_left && totype->type == ETPointer)
    )
    {
        if(a->op == 'e' || a->op == 'n')
        {
            l = LLVMBuildPtrToInt(cb->builder, l, LLVMInt64TypeInContext(utl_get_current_context()), "");
            r = LLVMBuildPtrToInt(cb->builder, r, LLVMInt64TypeInContext(utl_get_current_context()), "");

            a->resultantType = ett_base_type(ETInt1);

            LLVMIntPredicate p = a->op == 'e' ? LLVMIntEQ : LLVMIntNE;
            return LLVMBuildICmp(cb->builder, p, l, r, "");
        }

        EagleComplexType *lt = totype;
        EagleComplexType *rt = fromtype;
        if(a->op != '+' && a->op != '-' && a->op != 'e' && a->op !='P' && a->op != 'n')
            die(a->lineno, msgerr_op_invalid_for_ptr, a->op);
        if(lt->type == ETPointer && !ET_IS_INT(rt->type))
            die(a->lineno, msgerr_invalid_ptr_arithmetic);
        if(rt->type == ETPointer && !ET_IS_INT(lt->type))
            die(a->lineno, msgerr_invalid_ptr_arithmetic);

        if(lt->type == ETPointer && ett_get_base_type(lt) == ETAny && ett_pointer_depth(lt) == 1)
            die(a->lineno, msgerr_ptr_arithmetic_deref_any);
        if(rt->type == ETPointer && ett_get_base_type(rt) == ETAny && ett_pointer_depth(rt) == 1)
            die(a->lineno, msgerr_ptr_arithmetic_deref_any);

        LLVMValueRef indexer = lt->type == ETPointer ? r : l;
        LLVMValueRef ptr = lt->type == ETPointer ? l : r;

        if(a->op == '-')
            indexer = LLVMBuildNeg(cb->builder, indexer, "neg");

        EaglePointerType *pt = lt->type == ETPointer ?
            (EaglePointerType *)lt : (EaglePointerType *)rt;

        a->resultantType = (EagleComplexType *)pt;

        LLVMValueRef gep = LLVMBuildInBoundsGEP(cb->builder, ptr, &indexer, 1, "arith");
        return LLVMBuildBitCast(cb->builder, gep, ett_llvm_type((EagleComplexType *)pt), "cast");
    }

    switch(a->op)
    {
        case '+':
        case 'P':
            return ac_make_add(l, r, cb->builder, totype->type, a->lineno);
        case '-':
        case 'M':
            return ac_make_sub(l, r, cb->builder, totype->type, a->lineno);
        case '*':
        case 'T':
            return ac_make_mul(l, r, cb->builder, totype->type, a->lineno);
        case '/':
        case 'D':
            return ac_make_div(l, r, cb->builder, totype->type, a->lineno);
        case '%':
        case 'R':
            return ac_make_mod(l, r, cb->builder, totype->type, a->lineno);
        case 'o':
        case 'O':
            return ac_make_bitor(l, r, cb->builder, totype->type, a->lineno);
        case 'a':
        case 'A':
            return ac_make_bitand(l, r, cb->builder, totype->type, a->lineno);
        case '^':
        case 'X':
            return ac_make_bitxor(l, r, cb->builder, totype->type, a->lineno);
        case '>':
        case 'I':
            return ac_make_bitshift(l, r, cb->builder, totype->type, a->lineno, RIGHT);
        case '<':
        case 'E':
            return ac_make_bitshift(l, r, cb->builder, totype->type, a->lineno, LEFT);
        default:
            die(a->lineno, msgerr_invalid_binary_op, a->op);
            return NULL;
    }
}

LLVMValueRef ac_compile_binary(AST *ast, CompilerBundle *cb)
{
    ASTBinary *a = (ASTBinary *)ast;
    if(a->op == '=')
        return ac_build_store(ast, cb, 0);
    else if(a->op == '[')
        return ac_compile_index(ast, 0, cb);
    else if(a->op == '&')
        return ac_compile_logical_and(ast, cb);
    else if(a->op == '|')
        return ac_compile_logical_or(ast, cb);
    else if(a->op == 'P' || a->op == 'M' || a->op == 'T' || a->op == 'D' || a->op == 'R')
        return ac_build_store(ast, cb, 1);
    else if(a->op == 'A' || a->op == 'O' || a->op == 'X')
        return ac_build_store(ast, cb, 1);
    else if(a->op == 'I' || a->op == 'E')
        return ac_build_store(ast, cb, 1);

    LLVMValueRef l = ac_dispatch_expression(a->left, cb);

    if(a->left->resultantType->type == ETEnum)
        cb->enum_lookup = a->left->resultantType;
    LLVMValueRef r = ac_dispatch_expression(a->right, cb);
    cb->enum_lookup = NULL;

    if(a->left->resultantType->type == ETPointer || a->right->resultantType->type == ETPointer)
        return ac_generic_binary(a, l, r, 0, a->right->resultantType, a->left->resultantType, cb);

    EagleBasicType promo = et_promotion(a->left->resultantType->type, a->right->resultantType->type);
    a->resultantType = ett_base_type(promo);

    if(a->left->resultantType->type != promo)
    {
        l = ac_build_conversion(cb, l, a->left->resultantType, ett_base_type(promo), STRICT_CONVERSION, a->left->lineno);
    }
    else if(a->right->resultantType->type != promo)
    {
        r = ac_build_conversion(cb, r, a->right->resultantType, ett_base_type(promo), STRICT_CONVERSION, a->left->lineno);
    }

    switch(a->op)
    {
        case 'e':
        case 'n':
        case 'g':
        case 'l':
        case 'G':
        case 'L':
            a->resultantType = ett_base_type(ETInt1);
            return ac_make_comp(l, r, cb->builder, promo, a->op, ALN);
        default:
            return ac_generic_binary(a, l, r, 0, a->resultantType, a->resultantType, cb);
    }
}

LLVMValueRef ac_compile_get_address(AST *of, CompilerBundle *cb)
{
    switch(of->type)
    {
        case AIDENT:
        {
            ASTValue *o = (ASTValue *)of;
            VarBundle *b = vs_get(cb->varScope, o->value.id);

            if(!b)
                die(LN(of), msgerr_undeclared_identifier, o->value.id);

            of->resultantType = b->type;
            b->wasused = 1;

            return b->value;
        }
        case ABINARY:
        {
            ASTBinary *o = (ASTBinary *)of;
            if(o->op != '[')
                die(LN(of), msgerr_addr_invalid_expression);

            LLVMValueRef val = ac_compile_index(of, 1, cb);
            return val;
        }
        case ASTRUCTMEMBER:
        {
            return ac_compile_struct_member(of, cb, 1);
        }
        default:
            die(LN(of), msgerr_addr_invalid_expression);
    }

    return NULL;
}

LLVMValueRef ac_compile_unary(AST *ast, CompilerBundle *cb)
{
    ASTUnary *a = (ASTUnary *)ast;

    if(a->op == '&')
    {
        LLVMValueRef out = ac_compile_get_address(a->val, cb);
        a->resultantType = ett_pointer_type(a->val->resultantType);
        return out;
    }

    if(a->op == 't')
    {
        //ASTValue *v = (ASTValue *)a->val;
        //ac_replace_with_counted(cb, v->value.id);
        return NULL;
    }

    LLVMValueRef v;
    if(a->op != 's' && a->val)
        v = ac_dispatch_expression(a->val, cb);

    switch(a->op)
    {
        case 'p':
            {
                LLVMValueRef fmt = NULL;
                switch(a->val->resultantType->type)
                {
                    case ETFloat:
                        fmt = LLVMBuildGlobalStringPtr(cb->builder, "%f\n", "prfF");
                        v = LLVMBuildFPCast(cb->builder, v, LLVMDoubleTypeInContext(utl_get_current_context()), "bs");
                        break;
                    case ETDouble:
                        fmt = LLVMBuildGlobalStringPtr(cb->builder, "%lf\n", "prfLF");
                        break;
                    case ETInt1:
                        fmt = LLVMBuildGlobalStringPtr(cb->builder, "(Bool) %d\n", "prfB");
                        break;
                    case ETInt8:
                    case ETInt16:
                        v = LLVMBuildIntCast(cb->builder, v,
                                LLVMInt32TypeInContext(utl_get_current_context()), "bs");
                    case ETInt32:
                        fmt = LLVMBuildGlobalStringPtr(cb->builder, "%d\n", "prfI");
                        break;
                    case ETUInt8:
                    case ETUInt16:
                        v = LLVMBuildIntCast(cb->builder, v,
                                LLVMInt32TypeInContext(utl_get_current_context()), "bs");
                    case ETUInt32:
                        fmt = LLVMBuildGlobalStringPtr(cb->builder, "%u\n", "prfU");
                        break;
                    case ETUInt64:
                        fmt = LLVMBuildGlobalStringPtr(cb->builder, "%lu\n", "prfU");
                        break;
                    case ETInt64:
                        fmt = LLVMBuildGlobalStringPtr(cb->builder, "%ld\n", "prfLI");
                        break;
                    case ETFunction:
                    case ETArray:
                    case ETPointer:
                        {
                            EaglePointerType *pt = (EaglePointerType *)a->val->resultantType;
                            EaglePointerType *bytet = (EaglePointerType *)ett_pointer_type(ett_base_type(ETInt8));
                            LLVMValueRef cv = ac_try_view_conversion(cb, v, (EagleComplexType *)pt, (EagleComplexType *)bytet);

                            // We have a successful view conversion
                            if(cv)
                            {
                                v = cv;
                                pt = bytet;
                            }

                            fmt = LLVMBuildGlobalStringPtr(cb->builder, (pt->to->type == ETInt8 ? "%s\n" : "%p\n"), "prfPTR");
                        }
                        break;
                    default:
                        die(ALN, msgerr_invalid_puts_type);
                        break;
                }

                LLVMValueRef func = LLVMGetNamedFunction(cb->module, "printf");
                LLVMValueRef args[] = {fmt, v};
                LLVMBuildCall(cb->builder, func, args, 2, "putsout");
                return NULL;
            }
        case '*':
            {
                if(a->val->resultantType->type != ETPointer)
                    die(ALN, msgerr_invalid_deref_type);
                if(IS_ANY_PTR(a->val->resultantType))
                    die(ALN, msgerr_deref_any_no_cast);

                a->savedWrapped = v;
                ac_unwrap_pointer(cb, &v, a->val->resultantType, 0);

                EaglePointerType *pt = (EaglePointerType *)a->val->resultantType;
                a->resultantType = pt->to;

                LLVMValueRef r = v;
                if(a->resultantType->type != ETStruct && a->resultantType->type != ETClass && a->resultantType->type != ETInterface)
                    r = LLVMBuildLoad(cb->builder, v, "dereftmp");
                return r;
            }
            /*
        case 'c':
            {
                if(a->val->resultantType->type != ETArray)
                    die(ALN, msgerr_invalid_countof_type);

                LLVMValueRef r = LLVMBuildStructGEP(cb->builder, v, 0, "ctp");
                a->resultantType = ett_base_type(ETInt64);
                return LLVMBuildLoad(cb->builder, r, "dereftmp");
            }
            */
        /*
        case 'b':
            LLVMBuildBr(cb->builder, cb->currentLoopExit);
            break;
        case 'c':
            LLVMBuildBr(cb->builder, cb->currentLoopEntry);
            break;
            */
        case 'i':
            {
                if(!ET_IS_COUNTED(a->val->resultantType) && !ET_IS_WEAK(a->val->resultantType))
                    die(ALN, msgerr_invalid___inc_type);
                a->resultantType = a->val->resultantType;
                ac_incr_val_pointer(cb, &v, NULL);
                return v;
            }
        case 'd':
            {
                if(!ET_IS_COUNTED(a->val->resultantType) && !ET_IS_WEAK(a->val->resultantType))
                    die(ALN, msgerr_invalid___dec_type);
                a->resultantType = a->val->resultantType;
                ac_decr_val_pointer(cb, &v, NULL);
                return v;
            }
        case 'u':
            {
                if(!ET_IS_COUNTED(a->val->resultantType) && !ET_IS_WEAK(a->val->resultantType))
                    die(ALN, msgerr_invalid_unwrap_type);

                a->resultantType = ett_pointer_type(((EaglePointerType *)a->val->resultantType)->to);
                return LLVMBuildStructGEP(cb->builder, v, 5, "unwrap");
            }
        case 's':
            {
                ASTTypeDecl *aty = (ASTTypeDecl *)a->val;
                LLVMTypeRef ty = ett_llvm_type(aty->etype);

                unsigned long size = LLVMABISizeOfType(cb->td, ty);
                a->resultantType = ett_base_type(ETInt64);

                return LLVMConstInt(LLVMInt64TypeInContext(utl_get_current_context()), size, 0);
            }
        case '!':
            // TODO: Broken
            a->resultantType = ett_base_type(ETInt1);
            return ac_compile_ntest(a->val, v, cb);
        case '-':
        case '~':
            if(!ett_is_numeric(a->val->resultantType))
                die(ALN, msgerr_invalid_negate_type);
            a->resultantType = a->val->resultantType;
            return ac_generic_unary(a, v, cb);
        default:
            die(ALN, msgerr_invalid_unary_op, a->op);
            break;
    }

    return NULL;
}

LLVMValueRef ac_compile_generator_call(AST *ast, LLVMValueRef gen, CompilerBundle *cb)
{
    ASTFuncCall *a = (ASTFuncCall *)ast;
    gen = LLVMBuildStructGEP(cb->builder, gen, 5, ""); // Unwrap since it's counted
    LLVMValueRef clo = LLVMBuildStructGEP(cb->builder, gen, 0, "");
    LLVMValueRef func = LLVMBuildLoad(cb->builder, clo, "");

    EagleGenType *ett = (EagleGenType *)((EaglePointerType *)a->callee->resultantType)->to;
    EagleComplexType *ypt = ett_pointer_type(ett->ytype);

    LLVMTypeRef callee_types[] = {LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), ett_llvm_type(ypt)};
    func = LLVMBuildBitCast(cb->builder, func, LLVMPointerType(LLVMFunctionType(LLVMInt1TypeInContext(utl_get_current_context()), callee_types, 2, 0), 0), "");

    AST *p = a->params;

    LLVMValueRef val = ac_dispatch_expression(p, cb);
    EagleComplexType *rt = p->resultantType;

    if(!ett_are_same(rt, ett_pointer_type(ett->ytype)))
        val = ac_build_conversion(cb, val, rt, ett_pointer_type(ett->ytype), STRICT_CONVERSION, ALN);

    LLVMValueRef args[2];
    EaglePointerType *pt = (EaglePointerType *)rt;
    if(ET_IS_COUNTED(pt->to))
    {
        ac_decr_pointer(cb, &val, NULL);
    }

    args[0] = LLVMBuildBitCast(cb->builder, gen, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "");
    args[1] = val;

    LLVMValueRef out = LLVMBuildCall(cb->builder, func, args, 2, "callout");
    a->resultantType = ett_base_type(ETInt1);

    // hst_put(&cb->loadedTransients, ast, out, ahhd, ahed);

    return out;
}

LLVMValueRef ac_compile_function_call(AST *ast, CompilerBundle *cb)
{
    ASTFuncCall *a = (ASTFuncCall *)ast;

    LLVMValueRef func = ac_dispatch_expression(a->callee, cb);

    if(a->callee->resultantType->type == ETPointer)
    {
        if(((EaglePointerType *)a->callee->resultantType)->to->type == ETGenerator)
            return ac_compile_generator_call(ast, func, cb);
    }

    LLVMValueRef instanceOfClass = NULL;
    if(a->callee->type == ASTRUCTMEMBER)
    {
        ASTStructMemberGet *asmg = (ASTStructMemberGet *)a->callee;
        instanceOfClass = asmg->leftCompiled;
        asmg->leftCompiled = NULL;
    }

    EagleComplexType *orig = a->callee->resultantType;
    EagleFunctionType *ett;

    char *generic_call_name = NULL;

    // Deal with generic functions ... :(
    if(a->callee->type == AIDENT)
    {
        ASTValue *av = (ASTValue *)a->callee;
        if(ac_is_generic(av->value.id, cb))
        {
            generic_call_name = av->value.id;
        }
    }

    if(orig->type == ETFunction)
        ett = (EagleFunctionType *)orig;
    else
        ett = (EagleFunctionType *)((EaglePointerType *)orig)->to;

    a->resultantType = ett->retType;

    int start = instanceOfClass ? 1 : 0;
    int offset = instanceOfClass ? 0 : 1;

    AST *p;
    int i;
    for(p = a->params, i = start; p; p = p->next, i++);
    int ct = i;

    // Make sure the number of parameters matches...
    if(ct != ett->pct && !ett->variadic)
        die(ALN, msgerr_mismatch_param_count, ett->pct, ct);
    else if(ett->variadic && ct < ett->pct)
        die(ALN, msgerr_mismatch_param_count_variadic, ett->pct, ct);

    LLVMValueRef args[ct + offset];
    EagleComplexType *param_types[ct + offset];
    for(p = a->params, i = start; p; p = p->next, i++)
    {
        if(i < ett->pct && ett->params[i]->type == ETEnum)
            cb->enum_lookup = ett->params[i];

        LLVMValueRef val = ac_dispatch_expression(p, cb);
        EagleComplexType *rt = p->resultantType;

        cb->enum_lookup = NULL;

        if(i < ett->pct)
        {
            if(!ett_are_same(rt, ett->params[i]))
                val = ac_build_conversion(cb, val, rt, ett->params[i], LOOSE_CONVERSION, p->lineno);
        }

        hst_remove_key(&cb->transients, p, ahhd, ahed);
        // hst_remove_key(&cb->loadedTransients, p, ahhd, ahed);
        args[i + offset] = val;
        param_types[i + offset] = rt;
    }

    if(generic_call_name)
    {
        LLVMBasicBlockRef bb = LLVMGetInsertBlock(cb->builder);
        func = ac_generic_get(generic_call_name, param_types + 1, (EagleComplexType **)&ett, cb, ALN);
        ast->resultantType = ett->retType;
        LLVMPositionBuilderAtEnd(cb->builder, bb);
    }

    LLVMValueRef out;
    if(ET_IS_CLOSURE(ett))
    {
        // func = LLVMBuildLoad(cb->builder, func, "");
        if(!ET_IS_RECURSE(ett))
            ac_unwrap_pointer(cb, &func, NULL, 0);

        if(instanceOfClass)
            die(ALN, msgerr_internal);
        // if(ET_HAS_CLOASED(ett))
        args[0] = LLVMBuildLoad(cb->builder, LLVMBuildStructGEP(cb->builder, func, 1, ""), "");
        // else
        //     args[0] = LLVMConstPointerNull(LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0));


        func = LLVMBuildStructGEP(cb->builder, func, 0, "");
        func = LLVMBuildLoad(cb->builder, func, "");
        func = LLVMBuildBitCast(cb->builder, func, LLVMPointerType(ett_closure_type((EagleComplexType *)ett), 0), "");

        out = LLVMBuildCall(cb->builder, func, args, ct + 1, "");
    }
    else if(instanceOfClass)
    {
        // args[0] = LLVMBuildBitCast(cb->builder, instanceOfClass, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "");

        args[0] = instanceOfClass;
        out = LLVMBuildCall(cb->builder, func, args, ct, ett->retType->type == ETVoid ? "" : "callout");
    }
    else
        out = LLVMBuildCall(cb->builder, func, args + 1, ct, ett->retType->type == ETVoid ? "" : "callout");

    if((ET_IS_COUNTED(ett->retType)/* && ET_POINTEE(ett->retType)->type != ETGenerator*/) || (ett->retType->type == ETStruct && ty_needs_destructor(ett->retType)))
    {
        hst_put(&cb->loadedTransients, ast, out, ahhd, ahed);
    }

    return out;
}

void ac_set_static_initializer(int lineno, LLVMValueRef glob, LLVMValueRef init)
{
    if(!LLVMIsConstant(init))
        die(lineno, msgerr_global_init_non_constant);

    LLVMSetInitializer(glob, init);
}

LLVMValueRef ac_build_store(AST *ast, CompilerBundle *cb, char update)
{
    ASTBinary *a = (ASTBinary *)ast;
    EagleComplexType *totype;
    LLVMValueRef pos;

    VarBundle *storageBundle = NULL;
    char *storageIdent = NULL;
    int staticInitializer = 0;

    if(a->left->type == AIDENT)
    {
        ASTValue *l = (ASTValue *)a->left;
        VarBundle *b = vs_get(cb->varScope, l->value.id);

        if(ET_IS_CLOSED(b->type))
        {
            totype = ((EaglePointerType *)b->type)->to;
            pos = LLVMBuildLoad(cb->builder, b->value, "");
            pos = LLVMBuildStructGEP(cb->builder, pos, 5, "");
        }
        else
        {
            totype = b->type;
            pos = b->value;
        }

        storageIdent = l->value.id;
        storageBundle = b;
    }
    else if(a->left->type == AUNARY && ((ASTUnary *)a->left)->op == '*')
    {
        ASTUnary *l = (ASTUnary *)a->left;

        pos = ac_dispatch_expression(l->val, cb);
        if(l->val->resultantType->type != ETPointer)
        {
            fprintf(stderr, "Error: Only pointers may be dereferenced.\n");
            exit(1); }
        if(IS_ANY_PTR(l->val->resultantType))
        {
            fprintf(stderr, "Error: Any pointers may not be dereferenced without cast.\n");
            exit(1);
        }

        totype = ((EaglePointerType *)l->val->resultantType)->to;
        ac_unwrap_pointer(cb, &pos, l->val->resultantType, 0);
    }
    else if(a->left->type == AVARDECL)
    {
        pos = ac_dispatch_expression(a->left, cb);
        totype = a->left->resultantType;

        if(!pos)
        {
            storageIdent = ((ASTVarDecl *)a->left)->ident;
            storageBundle = vs_get(cb->varScope, storageIdent);
        }

        if(vl_is_static(((ASTVarDecl *)a->left)->linkage))
            staticInitializer = 1;
    }
    else if(a->left->type == ABINARY && ((ASTBinary *)a->left)->op == '[')
    {
        pos = ac_compile_index(a->left, 1, cb);
        totype = a->left->resultantType;
    }
    else if(a->left->type == ASTRUCTMEMBER)
    {
        pos = ac_compile_struct_member(a->left, cb, 1);
        totype = a->left->resultantType;
    }
    else
    {
        die(ALN, msgerr_invalid_assignment);
        return NULL;
    }

    // Deal with structure literals
    if(a->right->type == ASTRUCTLIT)
    {
        ASTStructLit *asl = (ASTStructLit *)a->right;

        if(totype->type == ETAuto)
        {
            if(!asl->name)
                die(ALN, msgerr_struct_literal_type_unknown);

            totype = ett_struct_type(asl->name);
            if(!storageBundle || !storageIdent)
                die(ALN, msgerr_internal);

            pos = ac_compile_var_decl_ext(totype, storageIdent, cb, 0, ALN);
            storageBundle->type = totype;
        }
        else if(totype->type == ETStruct)
        {
            if(!asl->name)
                asl->name = ((EagleStructType *)totype)->name;
        }
        else
            die(ALN, msgerr_invalid_struct_lit_assignment_val);

        ac_compile_struct_lit(a->right, cb, pos);
        return pos;
    }

    if(totype && totype->type == ETEnum)
        cb->enum_lookup = totype;
    LLVMValueRef r = ac_dispatch_expression(a->right, cb);
    cb->enum_lookup = NULL;
    EagleComplexType *fromtype = a->right->resultantType;

    // When pulling structure values out of arrays, we save the pointer so that the syntax
    // array[a][b].member = 5 works. But if we want to actually store that struct member
    // elsewhere, we need to load it in order to copy it.
    if(fromtype->type == ETStruct && LLVMTypeOf(r) == ett_llvm_type(ett_pointer_type(fromtype)))
        r = LLVMBuildLoad(cb->builder, r, "");

    if(totype->type == ETAuto)
    {
        totype = fromtype;
        if(!storageBundle || !storageIdent)
            die(ALN, msgerr_internal_storageBundle, storageBundle, storageIdent);

        pos = ac_compile_var_decl_ext(totype, storageIdent, cb, 0, ALN);
        storageBundle->type = totype;
    }

    if(storageBundle)
        storageBundle->wasassigned = 1;

    a->resultantType = totype;

    if(!ett_are_same(fromtype, totype) && staticInitializer)
    {
        r = ac_convert_const(r, totype, fromtype);
        if(!r)
            die(ALN, msgerr_invalid_conversion_constant);
    }
    else if(!ett_are_same(fromtype, totype) && (!update || (update && totype->type != ETPointer)))
        r = ac_build_conversion(cb, r, fromtype, totype, LOOSE_CONVERSION, a->right->lineno);

    if(update)
    {
        LLVMValueRef cur = LLVMBuildLoad(cb->builder, pos, "");
        r = ac_generic_binary(a, cur, r, 1, fromtype, totype, cb);
        // r = ac_make_add(cur, r, cb->builder, totype->type);
    }

    ac_safe_store(a->right, cb, pos, r, totype, staticInitializer, 1);

    return LLVMBuildLoad(cb->builder, pos, "loadtmp");
}

void ac_safe_store(AST *expr, CompilerBundle *cb, LLVMValueRef pos, LLVMValueRef val, EagleComplexType *totype, int staticInitializer, int deStruct)
{
    int transient = 0;
    if(ET_IS_COUNTED(totype))
    {
        if(expr)
        {
            hst_remove_key(&cb->transients, expr, ahhd, ahed);
            if(hst_remove_key(&cb->loadedTransients, expr, ahhd, ahed))
                transient = 1;
        }

        // Make sure we increment the new value before
        // the old value is decremented
        if(!transient)
            ac_incr_val_pointer(cb, &val, totype);

        ac_decr_pointer(cb, &pos, totype);
    }
    else if(ET_IS_WEAK(totype))
    {
        ac_remove_weak_pointer(cb, pos, totype);
        ac_add_weak_pointer(cb, val, pos, totype);
    }
    else if(totype->type == ETStruct && ty_needs_destructor(totype) && deStruct)
    {
        ac_call_destructor(cb, pos, totype);
        if(expr && hst_remove_key(&cb->loadedTransients, expr, ahhd, ahed))
            transient = 1;
    }

    // We are dealing with a static initializer for a static variable declaration
    // (a very specific case).
    if(staticInitializer)
        ac_set_static_initializer(expr->lineno, pos, val);
    else
        LLVMBuildStore(cb->builder, val, pos);

    if(totype->type == ETStruct && ty_needs_destructor(totype) && !transient && deStruct)
        ac_call_copy_constructor(cb, pos, totype);
}

