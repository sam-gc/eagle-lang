#include "ast_compiler.h"

int ac_compile_block(AST *ast, LLVMBasicBlockRef block, CompilerBundle *cb)
{
    for(; ast; ast = ast->next)
    {
        if(ast->type == AUNARY && ((ASTUnary *)ast)->op == 'r') // Handle the special return case
        {
            ASTUnary *reta = (ASTUnary *)ast;

            LLVMValueRef val = NULL;

            if(reta->val)
            {
                val = ac_dispatch_expression(reta->val, cb);
                EagleTypeType *t = reta->val->resultantType;
                EagleTypeType *o = cb->currentFunctionType->retType;

                if(!ett_are_same(t, o))
                    val = ac_build_conversion(cb->builder, val, t, o);
                if(t->type == ETStruct)
                    val = LLVMBuildLoad(cb->builder, val, "");

                if(ET_IS_COUNTED(o))
                {
                    ac_incr_val_pointer(cb, &val, o);

                    // We want to make sure that transients don't leak into returns (this only
                    // applies to closures since transients are implicitly destroyed on a normal
                    // function)

                    hst_remove_key(&cb->transients, ((ASTUnary *)ast)->val, ahhd, ahed);
                }
            }
            
            vs_run_callbacks_through(cb->varScope, cb->currentFunctionScope);

            if(val) LLVMBuildRet(cb->builder, val);
            else    LLVMBuildRetVoid(cb->builder);

            return 1;
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

void ac_compile_loop(AST *ast, CompilerBundle *cb)
{
    ASTLoopBlock *a = (ASTLoopBlock *)ast;
    LLVMBasicBlockRef testBB = LLVMAppendBasicBlock(cb->currentFunction, "test");
    LLVMBasicBlockRef loopBB = LLVMAppendBasicBlock(cb->currentFunction, "loop");
    LLVMBasicBlockRef incrBB = LLVMAppendBasicBlock(cb->currentFunction, "incr");
    LLVMBasicBlockRef cleanupBB = LLVMAppendBasicBlock(cb->currentFunction, "clean");
    LLVMBasicBlockRef mergeBB = LLVMAppendBasicBlock(cb->currentFunction, "merge");

    LLVMBasicBlockRef oldStart = cb->currentLoopEntry;
    LLVMBasicBlockRef oldExit = cb->currentLoopExit;

    cb->currentLoopEntry = incrBB;
    cb->currentLoopExit = cleanupBB;

    vs_push(cb->varScope);
    if(a->setup)
        ac_dispatch_expression(a->setup, cb);
    vs_push(cb->varScope);
    LLVMBuildBr(cb->builder, testBB);
    LLVMPositionBuilderAtEnd(cb->builder, testBB);
    LLVMValueRef val = ac_dispatch_expression(a->test, cb);
    LLVMValueRef cmp = ac_compile_test(a->test, val, cb);

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

    LLVMBasicBlockRef ifBB = LLVMAppendBasicBlock(cb->currentFunction, "if");
    LLVMBasicBlockRef elseBB = threeBlock || multiBlock ? LLVMAppendBasicBlock(cb->currentFunction, "else") : NULL;
    if(!mergeBB)
        mergeBB = LLVMAppendBasicBlock(cb->currentFunction, "merge");

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
        cmp = LLVMBuildICmp(cb->builder, LLVMIntEQ, val, LLVMConstInt(LLVMInt1Type(), 0, 0), "cmp");
    else if(res->resultantType->type == ETInt8)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntEQ, val, LLVMConstInt(LLVMInt8Type(), 0, 0), "cmp");
    else if(res->resultantType->type == ETInt32)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntEQ, val, LLVMConstInt(LLVMInt32Type(), 0, 0), "cmp");
    else if(res->resultantType->type == ETInt64)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntEQ, val, LLVMConstInt(LLVMInt64Type(), 0, 0), "cmp");
    else if(res->resultantType->type == ETDouble)
        cmp = LLVMBuildFCmp(cb->builder, LLVMRealOEQ, val, LLVMConstReal(LLVMDoubleType(), 0.0), "cmp");
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
        cmp = LLVMBuildICmp(cb->builder, LLVMIntNE, val, LLVMConstInt(LLVMInt1Type(), 0, 0), "cmp");
    else if(res->resultantType->type == ETInt8)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntNE, val, LLVMConstInt(LLVMInt8Type(), 0, 0), "cmp");
    else if(res->resultantType->type == ETInt32)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntNE, val, LLVMConstInt(LLVMInt32Type(), 0, 0), "cmp");
    else if(res->resultantType->type == ETInt64)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntNE, val, LLVMConstInt(LLVMInt64Type(), 0, 0), "cmp");
    else if(res->resultantType->type == ETDouble)
        cmp = LLVMBuildFCmp(cb->builder, LLVMRealONE, val, LLVMConstReal(LLVMDoubleType(), 0.0), "cmp");
    else if(res->resultantType->type == ETPointer)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntNE, val, LLVMConstPointerNull(ett_llvm_type(res->resultantType)), "cmp");
    else
        die(LN(res), "Cannot test against given type.");
    return cmp;
}
