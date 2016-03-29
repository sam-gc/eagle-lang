/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ast_compiler.h"

int ac_compile_block(AST *ast, LLVMBasicBlockRef block, CompilerBundle *cb)
{
    for(; ast; ast = ast->next)
    {
        if(ast->type == AUNARY && ((ASTUnary *)ast)->op == 'r') // Handle the special return case
        {
            ac_compile_return(ast, block, cb);
            return 1;
        }

        if(ast->type == AUNARY && ((ASTUnary *)ast)->op == 'y') // Handle the special yield case
        {
            ac_compile_yield(ast, block, cb);
            continue;
        }

        if(ast->type == AUNARY && ((ASTUnary *)ast)->op == 'b') // Handle the special break case
        {
            LLVMBuildBr(cb->builder, cb->currentLoopExit);
            return 1;
        }

        if(ast->type == AUNARY && ((ASTUnary *)ast)->op == 'c') // Handle the special continue case
        {
            LLVMBuildBr(cb->builder, cb->currentLoopEntry);
            return 1;
        }

        ac_dispatch_statement(ast, cb);
    }

    return 0;
}

void ac_compile_return(AST *ast, LLVMBasicBlockRef block, CompilerBundle *cb)
{
    ASTUnary *reta = (ASTUnary *)ast;

    LLVMValueRef val = NULL;

    if(reta->val)
    {
        if(cb->currentFunctionType->retType->type == ETEnum)
            cb->enum_lookup = cb->currentFunctionType->retType;

        val = ac_dispatch_expression(reta->val, cb);
        cb->enum_lookup = NULL;
        EagleTypeType *t = reta->val->resultantType;
        EagleTypeType *o = cb->currentFunctionType->retType;

        if(!ett_are_same(t, o))
            val = ac_build_conversion(cb, val, t, o, LOOSE_CONVERSION, ALN);
        if(t->type == ETStruct)
            val = LLVMBuildLoad(cb->builder, val, "");

        if(ET_IS_COUNTED(o))
        {
            ac_incr_val_pointer(cb, &val, o);

            // We want to make sure that transients don't leak into returns (this only
            // applies to closures since transients are implicitly destroyed on a normal
            // function)

            hst_remove_key(&cb->transients, ((ASTUnary *)ast)->val, ahhd, ahed);

            hst_for_each(&cb->loadedTransients, ac_decr_loaded_transients, cb);
            hst_free(&cb->loadedTransients);
            cb->loadedTransients = hst_create();
        }
    }

    vs_run_callbacks_through(cb->varScope, cb->currentFunctionScope);

    if(val) LLVMBuildRet(cb->builder, val);
    else    LLVMBuildRetVoid(cb->builder);
}

void ac_compile_yield(AST *ast, LLVMBasicBlockRef block, CompilerBundle *cb)
{
    ASTUnary *ya = (ASTUnary *)ast;

    LLVMValueRef val = NULL;

    if(!ya->val)
        die(ALN, "Yield statement must have an associated expression.");

    LLVMBasicBlockRef nblock = LLVMAppendBasicBlockInContext(utl_get_current_context(), cb->currentFunction, "yield");
    LLVMMoveBasicBlockAfter(nblock, cb->yieldBlocks->items[cb->yieldBlocks->count - 1]);
    arr_append(cb->yieldBlocks, nblock);

    if(cb->currentGenType->ytype->type == ETEnum)
        cb->enum_lookup = cb->currentGenType->ytype;

    val = ac_dispatch_expression(ya->val, cb);
    cb->enum_lookup = NULL;
    EagleTypeType *t = ya->val->resultantType;
    EagleTypeType *o = cb->currentGenType->ytype;

    if(!ett_are_same(t, o))
        val = ac_build_conversion(cb, val, t, o, LOOSE_CONVERSION, ya->val->lineno);

    if(ET_IS_COUNTED(o))
    {
        ac_incr_val_pointer(cb, &val, o);
    }

    LLVMValueRef ctx = LLVMBuildBitCast(cb->builder, LLVMGetParam(cb->currentFunction, 0),
        LLVMPointerType(ett_llvm_type((EagleTypeType *)cb->currentGenType), 0), "");

    LLVMBuildStore(cb->builder, val, LLVMGetParam(cb->currentFunction, 1));

    LLVMValueRef blockPos = LLVMBuildStructGEP(cb->builder, ctx, 1, "");
    LLVMBuildStore(cb->builder, LLVMBlockAddress(cb->currentFunction, nblock), blockPos);

    LLVMBuildRet(cb->builder, LLVMConstInt(LLVMInt1TypeInContext(utl_get_current_context()), 1, 0));

    LLVMPositionBuilderAtEnd(cb->builder, nblock);
}

void ac_compile_loop(AST *ast, CompilerBundle *cb)
{
    ASTLoopBlock *a = (ASTLoopBlock *)ast;
    LLVMBasicBlockRef testBB = LLVMAppendBasicBlockInContext(utl_get_current_context(), cb->currentFunction, "test");
    LLVMBasicBlockRef loopBB = LLVMAppendBasicBlockInContext(utl_get_current_context(), cb->currentFunction, "loop");
    LLVMBasicBlockRef incrBB = LLVMAppendBasicBlockInContext(utl_get_current_context(), cb->currentFunction, "incr");
    LLVMBasicBlockRef cleanupBB = LLVMAppendBasicBlockInContext(utl_get_current_context(), cb->currentFunction, "clean");
    LLVMBasicBlockRef mergeBB = LLVMAppendBasicBlockInContext(utl_get_current_context(), cb->currentFunction, "merge");

    LLVMBasicBlockRef oldStart = cb->currentLoopEntry;
    LLVMBasicBlockRef oldExit = cb->currentLoopExit;

    cb->currentLoopEntry = incrBB;
    cb->currentLoopExit = cleanupBB;

    // Stuff for iteration
    int rangeBased = a->setup && a->test && !a->update;
    LLVMValueRef gen, rawGen;
    gen = rawGen = NULL;
    LLVMValueRef iterator = NULL;
    EagleTypeType *ypt = NULL;

    vs_push(cb->varScope);
    if(a->setup)
    {
        iterator = ac_dispatch_expression(a->setup, cb);
        if(rangeBased)
        {
            gen = rawGen = ac_dispatch_expression(a->test, cb);

            if(!hst_remove_key(&cb->loadedTransients, a->test, ahhd, ahed))
                rawGen = NULL;

            if(a->test->resultantType->type != ETPointer ||
                (  ((EaglePointerType *)a->test->resultantType)->to->type != ETGenerator
                && ((EaglePointerType *)a->test->resultantType)->to->type != ETClass))
                die(ALN, "Range-based for-loops only work with generators.");

            EagleTypeType *setupt = ((EaglePointerType *)a->test->resultantType)->to;
            if(setupt->type == ETGenerator)
            {
                EagleGenType *gt = (EagleGenType *)setupt;
                if(!ett_are_same(gt->ytype, a->setup->resultantType))
                    die(ALN, "Generator type and iterator do not match.");
                ypt = ett_pointer_type(gt->ytype);
            }
            else
            {
                EagleTypeType *gt = ett_gen_type(a->setup->resultantType);
                EagleTypeType *pt = ett_pointer_type(gt);
                ((EaglePointerType *)pt)->counted = 1;
                rawGen = gen = ac_try_view_conversion(cb, gen, a->test->resultantType, pt);
                ypt = ett_pointer_type(a->setup->resultantType);
            }
        }
    }

    vs_push(cb->varScope);
    LLVMBuildBr(cb->builder, testBB);
    LLVMPositionBuilderAtEnd(cb->builder, testBB);

    AST *tmpr;

    LLVMValueRef val = NULL;
    if(rangeBased)
    {
        gen = LLVMBuildStructGEP(cb->builder, gen, 5, ""); // Unwrap since it's counted
        LLVMValueRef clo = LLVMBuildStructGEP(cb->builder, gen, 0, "");
        LLVMValueRef func = LLVMBuildLoad(cb->builder, clo, "");

        LLVMTypeRef callee_types[] = {LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), ett_llvm_type(ypt)};
        func = LLVMBuildBitCast(cb->builder, func, LLVMPointerType(LLVMFunctionType(LLVMInt1TypeInContext(utl_get_current_context()), callee_types, 2, 0), 0), "");

        LLVMValueRef args[2];
        args[0] = LLVMBuildBitCast(cb->builder, gen, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "");
        args[1] = iterator;

        val = LLVMBuildCall(cb->builder, func, args, 2, "iterout");

        tmpr = ast_make();
        tmpr->resultantType = ett_base_type(ETInt1);
    }
    else
    {
        val = ac_dispatch_expression(a->test, cb);
        tmpr = a->test;
    }
    LLVMValueRef cmp = ac_compile_test(tmpr, val, cb);

    LLVMBuildCondBr(cb->builder, cmp, loopBB, mergeBB);
    LLVMPositionBuilderAtEnd(cb->builder, loopBB);

    if(!ac_compile_block(a->block, loopBB, cb))
    {
        LLVMBuildBr(cb->builder, incrBB);
        LLVMPositionBuilderAtEnd(cb->builder, incrBB);
        if(a->update)
            ac_dispatch_expression(a->update, cb);

        vs_run_callbacks_through(cb->varScope, cb->varScope->scope);
        LLVMBuildBr(cb->builder, testBB);
        //LLVMDumpValue(LLVMBasicBlockAsValue(incrBB));

        LLVMPositionBuilderAtEnd(cb->builder, cleanupBB);
        vs_run_callbacks_through(cb->varScope, cb->varScope->scope);
        LLVMBuildBr(cb->builder, mergeBB);

        vs_pop(cb->varScope);
    }

    LLVMBasicBlockRef last = LLVMGetLastBasicBlock(cb->currentFunction);
    LLVMMoveBasicBlockAfter(incrBB, last);
    LLVMMoveBasicBlockAfter(cleanupBB, incrBB);
    LLVMMoveBasicBlockAfter(mergeBB, cleanupBB);

    LLVMPositionBuilderAtEnd(cb->builder, mergeBB);
    vs_run_callbacks_through(cb->varScope, cb->varScope->scope);
    if(rangeBased && rawGen)
        ac_decr_val_pointer(cb, &rawGen, a->test->resultantType);
    vs_pop(cb->varScope);

    cb->currentLoopEntry = oldStart;
    cb->currentLoopExit = oldExit;
}

void ac_compile_if(AST *ast, CompilerBundle *cb, LLVMBasicBlockRef mergeBB)
{
    ASTIfBlock *a = (ASTIfBlock *)ast;
    LLVMValueRef val = ac_dispatch_expression(a->test, cb);

    LLVMValueRef cmp = ac_compile_test(a->test, val, cb);

    int multiBlock = a->ifNext && ((ASTIfBlock *)a->ifNext)->test;
    int threeBlock = !!a->ifNext && !multiBlock;

    LLVMBasicBlockRef ifBB = LLVMAppendBasicBlockInContext(utl_get_current_context(), cb->currentFunction, "if");
    LLVMBasicBlockRef elseBB = threeBlock || multiBlock ? LLVMAppendBasicBlockInContext(utl_get_current_context(), cb->currentFunction, "else") : NULL;
    if(!mergeBB)
        mergeBB = LLVMAppendBasicBlockInContext(utl_get_current_context(), cb->currentFunction, "merge");

    LLVMBuildCondBr(cb->builder, cmp, ifBB, elseBB ? elseBB : mergeBB);
    LLVMPositionBuilderAtEnd(cb->builder, ifBB);

    vs_push(cb->varScope);
    if(!ac_compile_block(a->block, ifBB, cb))
    {
        vs_run_callbacks_through(cb->varScope, cb->varScope->scope);
        LLVMBuildBr(cb->builder, mergeBB);
    }
    vs_pop(cb->varScope);

    if(threeBlock)
    {
        ASTIfBlock *el = (ASTIfBlock *)a->ifNext;
        LLVMPositionBuilderAtEnd(cb->builder, elseBB);

        vs_push(cb->varScope);
        if(!ac_compile_block(el->block, elseBB, cb))
        {
            vs_run_callbacks_through(cb->varScope, cb->varScope->scope);
            LLVMBuildBr(cb->builder, mergeBB);
        }
        vs_pop(cb->varScope);
    }
    else if(multiBlock)
    {
        AST *el = a->ifNext;
        LLVMPositionBuilderAtEnd(cb->builder, elseBB);
        ac_compile_if(el, cb, mergeBB);

        return;
    }

    LLVMBasicBlockRef last = LLVMGetLastBasicBlock(cb->currentFunction);
    LLVMMoveBasicBlockAfter(mergeBB, last);

    LLVMPositionBuilderAtEnd(cb->builder, mergeBB);
}

LLVMValueRef ac_compile_ntest(AST *res, LLVMValueRef val, CompilerBundle *cb)
{
    LLVMValueRef cmp = NULL;
    if(res->resultantType->type == ETInt1)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntEQ, val, LLVMConstInt(LLVMInt1TypeInContext(utl_get_current_context()), 0, 0), "cmp");
    else if(res->resultantType->type == ETInt8)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntEQ, val, LLVMConstInt(LLVMInt8TypeInContext(utl_get_current_context()), 0, 0), "cmp");
    else if(res->resultantType->type == ETInt32)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntEQ, val, LLVMConstInt(LLVMInt32TypeInContext(utl_get_current_context()), 0, 0), "cmp");
    else if(res->resultantType->type == ETInt64)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntEQ, val, LLVMConstInt(LLVMInt64TypeInContext(utl_get_current_context()), 0, 0), "cmp");
    else if(res->resultantType->type == ETDouble)
        cmp = LLVMBuildFCmp(cb->builder, LLVMRealOEQ, val, LLVMConstReal(LLVMDoubleTypeInContext(utl_get_current_context()), 0.0), "cmp");
    else if(res->resultantType->type == ETPointer)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntEQ, val, LLVMConstPointerNull(ett_llvm_type(res->resultantType)), "cmp");
    else
        die(LN(res), "Cannot test against given type.");
    return cmp;
}

LLVMValueRef ac_compile_test(AST *res, LLVMValueRef val, CompilerBundle *cb)
{
    LLVMValueRef cmp = NULL;
    if(res->resultantType->type == ETInt1)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntNE, val, LLVMConstInt(LLVMInt1TypeInContext(utl_get_current_context()), 0, 0), "cmp");
    else if(res->resultantType->type == ETInt8)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntNE, val, LLVMConstInt(LLVMInt8TypeInContext(utl_get_current_context()), 0, 0), "cmp");
    else if(res->resultantType->type == ETInt32)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntNE, val, LLVMConstInt(LLVMInt32TypeInContext(utl_get_current_context()), 0, 0), "cmp");
    else if(res->resultantType->type == ETInt64)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntNE, val, LLVMConstInt(LLVMInt64TypeInContext(utl_get_current_context()), 0, 0), "cmp");
    else if(res->resultantType->type == ETDouble)
        cmp = LLVMBuildFCmp(cb->builder, LLVMRealONE, val, LLVMConstReal(LLVMDoubleTypeInContext(utl_get_current_context()), 0.0), "cmp");
    else if(res->resultantType->type == ETPointer)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntNE, val, LLVMConstPointerNull(ett_llvm_type(res->resultantType)), "cmp");
    else
        die(LN(res), "Cannot test against given type.");
    return cmp;
}
