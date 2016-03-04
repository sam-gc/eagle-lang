/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ast_compiler.h"
#include "core/utils.h"

// char *ac_generator_init_name(char *gen)
// {
//     char *name = malloc(strlen(gen) + 12);
//     sprintf(name, "__gen_%s_init", gen);

//     return name;
// }

char *ac_generator_context_name(char *gen)
{
    char *name = malloc(strlen(gen) + 12);
    sprintf(name, "__gen_%s_ctx", gen);

    return name;
}

char *ac_generator_code_name(char *gen)
{
    char *name = malloc(strlen(gen) + 12);
    sprintf(name, "__gen_%s_code", gen);

    return name;
}

char *ac_generator_destruct_name(char *gen)
{
    char *name = malloc(strlen(gen) + 12);
    sprintf(name, "__gen_%s_x", gen);

    return name;
}

LLVMValueRef ac_compile_generator_destructor(CompilerBundle *cb, GeneratorBundle *gb)
{
    LLVMTypeRef des_params[2];
    des_params[0] = LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0);
    des_params[1] = LLVMInt1TypeInContext(utl_get_current_context());

    char *desname = ac_generator_destruct_name(gb->ident);
    LLVMTypeRef des_func = LLVMFunctionType(LLVMVoidTypeInContext(utl_get_current_context()), des_params, 2, 0);
    LLVMValueRef func_des = LLVMAddFunction(cb->module, desname, des_func);
    free(desname);

    LLVMBasicBlockRef dentry = LLVMAppendBasicBlockInContext(utl_get_current_context(), func_des, "entry");

    LLVMPositionBuilderAtEnd(cb->builder, dentry);
    LLVMValueRef strct = LLVMBuildBitCast(cb->builder, LLVMGetParam(func_des, 0), LLVMPointerType(gb->countedContextType, 0), "");
    strct = LLVMBuildStructGEP(cb->builder, strct, 5, "");

    unsigned ct = LLVMCountStructElementTypes(gb->contextType);
    LLVMTypeRef tys[ct];
    LLVMGetStructElementTypes(gb->contextType, tys);

    unsigned i;
    for(i = 2; i < ct; i++)
    {
        if(LLVMGetTypeKind(tys[i]) == LLVMPointerTypeKind)
        {
            LLVMValueRef pos = LLVMBuildStructGEP(cb->builder, strct, i, "");
            ac_decr_pointer(cb, &pos, NULL);
        }
    }

    LLVMBuildRetVoid(cb->builder);

    return func_des;
}

LLVMValueRef ac_compile_generator_init(AST *ast, CompilerBundle *cb, GeneratorBundle *gb)
{
    ASTFuncDecl *a = (ASTFuncDecl *)ast;
    VarBundle *vb = vs_get(cb->varScope, a->ident);
    LLVMValueRef func = vb->value;

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(utl_get_current_context(), func, "entry");
    LLVMPositionBuilderAtEnd(cb->builder, entry);

    EagleTypeType **eparam_types = gb->eparam_types;

    cb->currentFunctionEntry = entry;
    cb->currentFunction = func;
    cb->currentFunctionScope = cb->varScope->scope;

    LLVMValueRef mmc = ac_compile_malloc_counted_raw(gb->contextType, &gb->countedContextType, cb);
    LLVMValueRef ctx = LLVMBuildStructGEP(cb->builder, mmc, 5, "");

    LLVMValueRef pos = LLVMBuildStructGEP(cb->builder, ctx, 0, "");
    LLVMBuildStore(cb->builder, LLVMBuildBitCast(cb->builder, gb->func, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), ""), pos);

    pos = LLVMBuildStructGEP(cb->builder, ctx, 1, "");
    LLVMBuildStore(cb->builder, LLVMBlockAddress(gb->func, cb->yieldBlocks->items[0]), pos);

    ac_incr_val_pointer(cb, &mmc, ((EagleFunctionType *)vb->type)->retType);

    pos = LLVMBuildStructGEP(cb->builder, mmc, 4, "");

    LLVMValueRef des_func = ac_compile_generator_destructor(cb, gb);
    LLVMPositionBuilderAtEnd(cb->builder, entry);
    LLVMBuildStore(cb->builder, des_func, pos);


    // Null out the references in the context in case the generator is never used
    unsigned ct = LLVMCountStructElementTypes(gb->contextType);
    LLVMTypeRef tys[ct];
    LLVMGetStructElementTypes(gb->contextType, tys);

    unsigned i;
    for(i = 2; i < ct; i++)
    {
        if(LLVMGetTypeKind(tys[i]) == LLVMPointerTypeKind)
        {
            LLVMValueRef pos = LLVMBuildStructGEP(cb->builder, ctx, i, "");
            LLVMBuildStore(cb->builder, LLVMConstPointerNull(tys[i]), pos);
        }
    }
    // ==========================================================================

    if(a->params)
    {
        int i;
        AST *p = a->params;
        for(i = 0; p; p = p->next, i++)
        {
            EagleTypeType *ty = eparam_types[i];
            // LLVMValueRef pos = ac_compile_var_decl(p, cb);
            LLVMValueRef pos = LLVMBuildStructGEP(cb->builder, ctx, (int)(uintptr_t)hst_get(gb->params, (void *)(uintptr_t)(i + 1), ahhd, ahed), "");

            LLVMBuildStore(cb->builder, LLVMGetParam(func, i), pos);

            if(ET_IS_COUNTED(ty))
                ac_incr_pointer(cb, &pos, eparam_types[i]);
            if(ET_IS_WEAK(ty))
                ac_add_weak_pointer(cb, LLVMGetParam(func, i), pos, eparam_types[i]);
        }
    }

    // LLVMDumpType(ett_llvm_type(vb->type));
    LLVMValueRef ret = LLVMBuildBitCast(cb->builder, mmc, ett_llvm_type(((EagleFunctionType *)vb->type)->retType), "");
    LLVMBuildRet(cb->builder, ret);

    return NULL;
}

void ac_compile_generator_code(AST *ast, CompilerBundle *cb)//, LLVMValueRef func, EagleFunctionType *ft)
{
    ASTFuncDecl *a = (ASTFuncDecl *)ast;

    arraylist param_values = arr_create(10);
    arraylist yield_blocks = arr_create(10);

    ASTTypeDecl *genType = (ASTTypeDecl *)a->retType;
    cb->currentGenType = (EagleGenType *)ett_gen_type(genType->etype);

    char *codename = ac_generator_code_name(a->ident);

    LLVMTypeRef pts[] = {LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), LLVMPointerType(ett_llvm_type(genType->etype), 0)};
    LLVMTypeRef funcType = LLVMFunctionType(LLVMInt1TypeInContext(utl_get_current_context()), pts, 2, 0);
    LLVMValueRef func = LLVMAddFunction(cb->module, codename, funcType);
    free(codename);

    // If we have an extern generator, we will stop
    // the compilation here.
    if(!a->body)
        return;

    int i;
    AST *p = a->params;
    for(i = 0; p; p = p->next, i++);

    int ct = i;

    EagleTypeType *eparam_types[ct];
    for(i = 0, p = a->params; i < ct; p = p->next, i++)
    {
        ASTTypeDecl *type = (ASTTypeDecl *)((ASTVarDecl *)p)->atype;
        eparam_types[i] = type->etype;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(utl_get_current_context(), func, "entry");
    LLVMPositionBuilderAtEnd(cb->builder, entry);

    vs_push(cb->varScope);

    cb->currentFunctionEntry = entry;
    cb->currentFunction = func;
    cb->currentFunctionScope = cb->varScope->scope;
    cb->yieldBlocks = &yield_blocks;

    hashtable param_mapping = hst_create();

    if(a->params)
    {
        int i;
        AST *p = a->params;
        for(i = 0; p; p = p->next, i++)
        {
            // EagleTypeType *ty = eparam_types[i];
            ((ASTVarDecl *)p)->noSetNil = 1;
            LLVMValueRef pos = ac_compile_var_decl(p, cb);

            arr_append(&param_values, pos);
            hst_put(&param_mapping, pos, (void *)(uintptr_t)(i + 1), ahhd, ahed);

            // LLVMBuildStore(cb->builder, LLVMGetParam(func, i), pos);
            // if(ET_IS_COUNTED(ty))
            //     ac_incr_pointer(cb, &pos, eparam_types[i]);
            // if(ET_IS_WEAK(ty))
            //     ac_add_weak_pointer(cb, LLVMGetParam(func, i), pos, eparam_types[i]);
        }
    }

    LLVMBasicBlockRef fy = LLVMAppendBasicBlockInContext(utl_get_current_context(), func, "yield");
    LLVMPositionBuilderAtEnd(cb->builder, fy);
    arr_append(cb->yieldBlocks, fy);

    ac_compile_block(a->body, NULL, cb);

    // if(!ac_compile_block(a->body, entry, cb) && retType->etype->type != ETVoid)
    //     die(ALN, "Function must return a value.");

    // if(retType->etype->type == ETVoid)
    // {
    vs_run_callbacks_through(cb->varScope, cb->varScope->scope);
    vs_pop(cb->varScope);

    char *ctxname = ac_generator_context_name(a->ident);
    LLVMTypeRef ctx = LLVMStructCreateNamed(utl_get_current_context(), ctxname);
    free(ctxname);

    // arraylist param_indices = arr_create(10);
    hashtable prms = hst_create();

    GeneratorBundle gb;
    gb.ident = a->ident;
    gb.func = func;
    gb.contextType = ctx;
    // gb.param_indices = &param_indices;
    // gb.params = &param_values;
    gb.param_mapping = &param_mapping;
    gb.params = &prms;
    gb.eparam_types = eparam_types;
    gb.epct = ct;
    gb.last_block = LLVMGetInsertBlock(cb->builder);

    ac_generator_replace_allocas(cb, &gb);
    LLVMPositionBuilderAtEnd(cb->builder, gb.last_block);

    LLVMBuildRet(cb->builder, LLVMConstInt(LLVMInt1TypeInContext(utl_get_current_context()), 0, 0));

    //     LLVMBuildRetVoid(cb->builder);
    // }

    ac_compile_generator_init(ast, cb, &gb);

    // ac_dump_allocas(cb->currentFunctionEntry, cb);

    // arr_free(&param_indices);
    arr_free(&param_values);
    arr_free(&yield_blocks);
    hst_free(&param_mapping);
    hst_free(&prms);

    cb->currentFunctionEntry = NULL;

    // EagleTypeType *types[] = {ett_pointer_type(ett_base_type(ETInt8)), ett_pointer_type(ett_base_type(ETInt32))};
    // vs_put(cb->varScope, "__gen_test_code", func, ett_function_type(ett_base_type(ETInt1), types, 2));
}

void ac_generator_replace_allocas(CompilerBundle *cb, GeneratorBundle *gb)
{
    LLVMBasicBlockRef entry = cb->currentFunctionEntry;

    arraylist elems = arr_create(10);
    arr_append(&elems, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0));
    arr_append(&elems, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0));

    LLVMValueRef first = LLVMGetFirstInstruction(entry);
    LLVMValueRef btb = NULL;

    if(first)
    {
        LLVMValueRef i;
        for(i = first; i; i = LLVMGetNextInstruction(i))
        {
            if(LLVMGetInstructionOpcode(i) != LLVMAlloca)
                continue;

            LLVMTypeRef type = LLVMGetElementType(LLVMTypeOf(i));
            arr_append(&elems, type);
        }

        LLVMTypeRef ctx = gb->contextType;
        LLVMStructSetBody(ctx, (LLVMTypeRef *)elems.items, elems.count, 0);

        LLVMPositionBuilderBefore(cb->builder, first);
        LLVMValueRef par = LLVMGetParam(gb->func, 0);
        btb = LLVMBuildBitCast(cb->builder, par, LLVMPointerType(ctx, 0), "");
        // LLVMValueRef btb = LLVMBuildAlloca(cb->builder, ctx, "btb");

        // LLVMDumpType(ctx);

        hashtable *pm = gb->param_mapping;
        hashtable *po = gb->params;

        gb->elems = &elems;

        ac_null_out_counted(cb, gb, btb);

        arr_free(&elems);
        elems = arr_create(10);

        LLVMPositionBuilderBefore(cb->builder, first);

        int c;
        for(i = first, c = 2; i; i = LLVMGetNextInstruction(i))
        {
            if(LLVMGetInstructionOpcode(i) != LLVMAlloca || i == btb)
                continue;

            LLVMValueRef pos = LLVMBuildStructGEP(cb->builder, btb, c, "");
            LLVMReplaceAllUsesWith(i, pos);

            int old = 0;
            if((old = (int)(uintptr_t)hst_get(pm, i, ahhd, ahed)))
            {
                hst_put(po, (void *)(uintptr_t)old, (void *)(uintptr_t)c, ahhd, ahed);
            }

            // LLVMTypeRef type = LLVMGetElementType(LLVMTypeOf(i));
            arr_append(&elems, i);

            c++;
        }

        for(c = 0; c < elems.count; c++)
        {
            LLVMInstructionEraseFromParent(elems.items[c]);
        }
    }
    else
    {
        LLVMTypeRef ctx = gb->contextType;
        LLVMStructSetBody(ctx, (LLVMTypeRef *)elems.items, elems.count, 0);

        LLVMPositionBuilderAtEnd(cb->builder, entry);
        LLVMValueRef par = LLVMGetParam(gb->func, 0);
        btb = LLVMBuildBitCast(cb->builder, par, LLVMPointerType(ctx, 0), "");
    }

    LLVMPositionBuilderAtEnd(cb->builder, cb->currentFunctionEntry);
    LLVMValueRef jumpps = LLVMBuildLoad(cb->builder, LLVMBuildStructGEP(cb->builder, btb, 1, ""), "");
    LLVMValueRef jmp = LLVMBuildIndirectBr(cb->builder, jumpps, cb->yieldBlocks->count);

    int c;
    for(c = 0; c < cb->yieldBlocks->count; c++)
        LLVMAddDestination(jmp, cb->yieldBlocks->items[c]);

    arr_free(&elems);
}

void ac_null_out_counted(CompilerBundle *cb, GeneratorBundle *gb, LLVMValueRef btb)
{
    LLVMBasicBlockRef curr = LLVMGetInsertBlock(cb->builder);
    LLVMPositionBuilderAtEnd(cb->builder, gb->last_block);
    arraylist *elems = gb->elems;
    int i;
    for(i = 2; i < elems->count; i++)
    {
        LLVMTypeRef e = arr_get(elems, i);
        if(LLVMGetTypeKind(e) == LLVMPointerTypeKind)
        {
            LLVMValueRef pos = LLVMBuildStructGEP(cb->builder, btb, i, "");
            LLVMBuildStore(cb->builder, LLVMConstPointerNull(e), pos);
        }
    }

    LLVMPositionBuilderAtEnd(cb->builder, curr);
}

void ac_add_gen_declaration(AST *ast, CompilerBundle *cb)
{
    ASTFuncDecl *a = (ASTFuncDecl *)ast;

    int i, ct;
    AST *p;
    for(p = a->params, i = 0; p; p = p->next, i++);
    ct = i;

    LLVMTypeRef param_types[ct];
    EagleTypeType *eparam_types[ct];
    for(i = 0, p = a->params; p; p = p->next, i++)
    {
        ASTTypeDecl *type = (ASTTypeDecl *)((ASTVarDecl *)p)->atype;

        param_types[i] = ett_llvm_type(type->etype);
        eparam_types[i] = type->etype;

        if(type->etype->type == ETStruct)
            die(ALN, "Passing struct by value not supported.");
    }

    ASTTypeDecl *genType = (ASTTypeDecl *)a->retType;

    EagleTypeType *ty = ett_pointer_type(ett_gen_type(genType->etype));
    ((EaglePointerType *)ty)->counted = 1;

    LLVMTypeRef func_type = LLVMFunctionType(ett_llvm_type(ty), param_types, ct, a->vararg);
    LLVMValueRef func = LLVMAddFunction(cb->module, a->ident, func_type);

    vs_put(cb->varScope, a->ident, func, ett_function_type(ty, eparam_types, ct), -1);
}

// void ac_compile_generator(AST *ast, CompilerBundle *cb)
// {
//     LLVMValueRef func = NULL;

//     ASTFuncDecl *a = (ASTFuncDecl *)ast;

//     VarBundle *vb = vs_get(cb->varScope, a->ident);
//     func = vb->value;

//     ac_compile_generator_ex(ast, cb, func, (EagleFunctionType *)vb->type);
// }
