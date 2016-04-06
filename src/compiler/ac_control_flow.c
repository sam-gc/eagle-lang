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
        if(ast->type == AUNARY)
        {
            ASTUnary *un = (ASTUnary *)ast;
            switch(un->op)
            {
                case 'r': // Return
                    ac_compile_return(ast, block, cb);
                    return 1;
                case 'y': // Yield
                    ac_compile_yield(ast, block, cb);
                    continue;
                case 'b': // Break
                    LLVMBuildBr(cb->builder, cb->currentLoopExit);
                    return 1;
                case 'c': // Continue
                    LLVMBuildBr(cb->builder, cb->currentLoopEntry);
                    return 1;
                case 'f': // Fallthrough
                    if(!cb->nextCaseBlock)
                        die(un->lineno, "Attempting a fallthrough outside of switch statement");
                    vs_run_callbacks_through(cb->varScope, cb->currentFunctionScope);
                    LLVMBuildBr(cb->builder, cb->nextCaseBlock);
                    return 1;
            }
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

int ac_type_is_valid_for_switch(EagleTypeType *type)
{
    switch(type->type)
    {
        case ETInt1:
        case ETInt8:
        case ETInt32:
        case ETInt64:
        case ETEnum:
            return 1;
        default:
            return 0;
    }
}

static int ac_count_switch_cases(ASTSwitchBlock *ast)
{
    AST *cblock = ast->cases;
    int i;
    for(i = 0; cblock; i += 1, cblock = cblock->next);
    return i;
}

static LLVMValueRef ac_compile_case_label(AST *ast, CompilerBundle *cb)
{
    ASTCaseBlock *cblock = (ASTCaseBlock *)ast;

    if(!cblock->targ)
        return NULL;

    LLVMValueRef label = ac_dispatch_expression(cblock->targ, cb);
    
    EagleTypeType *rt = cblock->targ->resultantType;
    if(!ac_type_is_valid_for_switch(rt))
        die(ALN, "Case branch not constant");
    if(!LLVMIsConstant(label))
        die(ALN, "Case branch not constant");

    return label;
}

static void ac_check_case_label_uniqueness(LLVMValueRef label, long long *cases, int ct, int lineno)
{
    long long num = LLVMConstIntGetSExtValue(label);
    for(int i = 0; i < ct; i++)
    {
        if(cases[i] == num)
            die(lineno, "Duplicate case label");
    }

    cases[ct] = num;
}

void ac_compile_switch(AST *ast, CompilerBundle *cb)
{
    ASTSwitchBlock *a = (ASTSwitchBlock *)ast;

    // Save the old fall-through block value
    LLVMBasicBlockRef old_nextCaseBlock = cb->nextCaseBlock;

    int case_count = ac_count_switch_cases(a);

    LLVMBasicBlockRef caseBlocks[case_count];
    long long caseLabels[case_count];

    for(int i = 0; i < case_count; i++)
        caseBlocks[i] = LLVMAppendBasicBlockInContext(utl_get_current_context(), cb->currentFunction, "case");

    LLVMBasicBlockRef mergeBB = LLVMAppendBasicBlockInContext(utl_get_current_context(), cb->currentFunction, "merge");

    LLVMValueRef test = ac_dispatch_expression(a->test, cb);
    ac_flush_transients(cb);

    EagleTypeType *test_type = a->test->resultantType;
    if(!ac_type_is_valid_for_switch(test_type))
        die(ALN, "Cannot switch over given type");

    AST *cblock = a->cases;

    LLVMBasicBlockRef dumpBB;
    if(a->default_index < 0)
        dumpBB = mergeBB;
    else
        dumpBB = caseBlocks[a->default_index];

    LLVMValueRef swtch = LLVMBuildSwitch(cb->builder, test, dumpBB, case_count);
    for(int i = 0; i < case_count; i++)
    {
        if(i != a->default_index)
        {
            if(test_type->type == ETEnum)
                cb->enum_lookup = test_type;
    
            LLVMValueRef targ = ac_compile_case_label(cblock, cb);
            cb->enum_lookup = NULL;

            ac_check_case_label_uniqueness(targ, caseLabels, i, cblock->lineno);
            LLVMAddCase(swtch, targ, caseBlocks[i]);
        }

        vs_push(cb->varScope);
        LLVMPositionBuilderAtEnd(cb->builder, caseBlocks[i]);

        cb->nextCaseBlock = i == case_count - 1 ? mergeBB : caseBlocks[i + 1];

        if(!ac_compile_block(((ASTCaseBlock *)cblock)->body, caseBlocks[i], cb))
        {
            vs_run_callbacks_through(cb->varScope, cb->varScope->scope);
            LLVMBuildBr(cb->builder, mergeBB);
        }
        vs_pop(cb->varScope);

        cblock = cblock->next;
    }

    LLVMPositionBuilderAtEnd(cb->builder, mergeBB);
    
    // Restore the old next case block value
    cb->nextCaseBlock = old_nextCaseBlock;
}

LLVMValueRef ac_compile_ternary(AST *ast, CompilerBundle *cb)
{
    ASTTernary *a = (ASTTernary *)ast;
    AST *tree_ifYes = a->ifyes;
    AST *tree_ifNo  = a->ifno;

    AST *yes_tree = tree_ifYes ? tree_ifYes : a->test;

    // Compile the test condition
    LLVMValueRef test_raw = ac_dispatch_expression(a->test, cb);
    LLVMValueRef test = ac_compile_test(a->test, test_raw, cb);

    // Add the blocks to build
    LLVMBasicBlockRef ifyesBB = LLVMAppendBasicBlockInContext(utl_get_current_context(), cb->currentFunction, "ifyes");
    LLVMBasicBlockRef ifnoBB  = LLVMAppendBasicBlockInContext(utl_get_current_context(), cb->currentFunction, "ifno");
    LLVMBasicBlockRef mergeBB = LLVMAppendBasicBlockInContext(utl_get_current_context(), cb->currentFunction, "merge");

    LLVMBuildCondBr(cb->builder, test, ifyesBB, ifnoBB);

    // Generate the values
    LLVMPositionBuilderAtEnd(cb->builder, ifyesBB);

    // Handle elvis operator (?:)
    LLVMValueRef valA = tree_ifYes ? ac_dispatch_expression(tree_ifYes, cb) : test_raw;
    
    LLVMPositionBuilderAtEnd(cb->builder, ifnoBB);
    LLVMValueRef valB = ac_dispatch_expression(tree_ifNo, cb);

    EagleTypeType *ytype = tree_ifYes ? tree_ifYes->resultantType : a->test->resultantType;
    EagleTypeType *ntype = tree_ifNo->resultantType;

    // If the types are not the same, we will try to run a conversion. The resultant
    // type is assumed to be the type of the _yes_ branch. As such, we will build any
    // conversion code in the _no_ branch, which the builder is still positioned on
    if(!ett_are_same(ytype, ntype))
        valB = ac_build_conversion(cb, valB, ntype, ytype, LOOSE_CONVERSION, tree_ifNo->lineno);

    // Now we need to build a temporary alloca to store the result
    LLVMPositionBuilderAtEnd(cb->builder, cb->currentFunctionEntry);
    LLVMValueRef begin = LLVMGetFirstInstruction(cb->currentFunctionEntry);
    if(begin)
        LLVMPositionBuilderBefore(cb->builder, begin);

    LLVMValueRef output = LLVMBuildAlloca(cb->builder, ett_llvm_type(ytype), "ternary.tmp");

    // We need to deal with transient allocations at this point
    int need_loaded = 0;
    int need_transients = 0;

    need_loaded = hst_contains_key(&cb->loadedTransients, yes_tree, ahhd, ahed) ||
                  hst_contains_key(&cb->loadedTransients, tree_ifNo, ahhd, ahed);
    need_transients = need_loaded || hst_contains_key(&cb->transients, yes_tree, ahhd, ahed) ||
                                     hst_contains_key(&cb->transients, tree_ifNo, ahhd, ahed);

    if(need_loaded)
    {
        LLVMPositionBuilderAtEnd(cb->builder, ifyesBB);
        if(!hst_remove_key(&cb->loadedTransients, yes_tree, ahhd, ahed))
            ac_incr_val_pointer(cb, &valA, ytype);
        LLVMPositionBuilderAtEnd(cb->builder, ifnoBB);
        if(!hst_remove_key(&cb->loadedTransients, tree_ifNo, ahhd, ahed))
            ac_incr_val_pointer(cb, &valB, ytype);
    }

    hst_remove_key(&cb->transients, yes_tree, ahhd, ahed);
    hst_remove_key(&cb->transients, tree_ifNo, ahhd, ahed);

    LLVMPositionBuilderAtEnd(cb->builder, mergeBB);
    LLVMValueRef opened_output = LLVMBuildLoad(cb->builder, output, "ternary.output");

    if(need_loaded)
        hst_put(&cb->loadedTransients, ast, opened_output, ahhd, ahed);
    else if(need_transients)
        hst_put(&cb->transients, ast, opened_output, ahhd, ahed);

    // Now we will go back and assign those temp values to the alloc'd variable
    LLVMPositionBuilderAtEnd(cb->builder, ifyesBB);
    LLVMBuildStore(cb->builder, valA, output);
    LLVMBuildBr(cb->builder, mergeBB);

    LLVMPositionBuilderAtEnd(cb->builder, ifnoBB);
    LLVMBuildStore(cb->builder, valB, output);
    LLVMBuildBr(cb->builder, mergeBB);

    LLVMPositionBuilderAtEnd(cb->builder, mergeBB);

    ast->resultantType = ytype;
    return opened_output;
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
    EagleTypeType *rt = res->resultantType;

    if(ET_IS_LLVM_INT(rt->type))
        cmp = LLVMBuildICmp(cb->builder, LLVMIntEQ, val, LLVMConstInt(ett_llvm_type(rt), 0, 0), "cmp");
    else if(ET_IS_REAL(rt->type))
        cmp = LLVMBuildFCmp(cb->builder, LLVMRealOEQ, val, LLVMConstReal(ett_llvm_type(rt), 0.0), "cmp");
    else if(res->resultantType->type == ETPointer)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntEQ, val, LLVMConstPointerNull(ett_llvm_type(res->resultantType)), "cmp");
    else
        die(LN(res), "Cannot test against given type.");

    return cmp;
}

LLVMValueRef ac_compile_test(AST *res, LLVMValueRef val, CompilerBundle *cb)
{
    LLVMValueRef cmp = NULL;
    EagleTypeType *rt = res->resultantType;

    if(ET_IS_LLVM_INT(rt->type))
        cmp = LLVMBuildICmp(cb->builder, LLVMIntNE, val, LLVMConstInt(ett_llvm_type(rt), 0, 0), "cmp");
    else if(ET_IS_REAL(rt->type))
        cmp = LLVMBuildFCmp(cb->builder, LLVMRealONE, val, LLVMConstReal(ett_llvm_type(rt), 0.0), "cmp");
    else if(res->resultantType->type == ETPointer)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntNE, val, LLVMConstPointerNull(ett_llvm_type(res->resultantType)), "cmp");
    else
        die(LN(res), "Cannot test against given type.");

    return cmp;
}

