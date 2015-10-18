#include "ast_compiler.h"

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
// LLVMValueRef ac_compile_malloc_counted_raw(LLVMTypeRef rt, LLVMTypeRef *out, CompilerBundle *cb)
LLVMValueRef ac_compile_generator_init(AST *ast, CompilerBundle *cb, GeneratorBundle *gb)
{
    ASTFuncDecl *a = (ASTFuncDecl *)ast;
    VarBundle *vb = vs_get(cb->varScope, a->ident);
    LLVMValueRef func = vb->value;

    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(func, "entry");
    LLVMPositionBuilderAtEnd(cb->builder, entry);

    EagleTypeType **eparam_types = gb->eparam_types;

    cb->currentFunctionEntry = entry;
    cb->currentFunction = func;
    cb->currentFunctionScope = cb->varScope->scope;

    LLVMTypeRef outCounted;
    LLVMValueRef mmc = ac_compile_malloc_counted_raw(gb->contextType, &outCounted, cb);
    LLVMValueRef ctx = LLVMBuildStructGEP(cb->builder, mmc, 5, "");

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

    LLVMValueRef pos = LLVMBuildStructGEP(cb->builder, ctx, 0, "");
    LLVMBuildStore(cb->builder, LLVMBuildBitCast(cb->builder, gb->func, LLVMPointerType(LLVMInt8Type(), 0), ""), pos);

    LLVMValueRef ret = LLVMBuildBitCast(cb->builder, mmc, LLVMPointerType(LLVMInt8Type(), 0), "");
    LLVMBuildRet(cb->builder, ret);

    return NULL;
}

void ac_compile_generator_code(AST *ast, CompilerBundle *cb)//, LLVMValueRef func, EagleFunctionType *ft)
{
    ASTFuncDecl *a = (ASTFuncDecl *)ast;
    arraylist param_values = arr_create(10);

    char *codename = ac_generator_code_name(a->ident);

    LLVMTypeRef pt = LLVMPointerType(LLVMInt8Type(), 0);
    LLVMTypeRef funcType = LLVMFunctionType(LLVMInt1Type(), &pt, 1, 0);
    LLVMValueRef func = LLVMAddFunction(cb->module, codename, funcType);
    free(codename);
    
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

    // ASTTypeDecl *genType = (ASTTypeDecl *)a->retType;

    
    // cb->currentFunctionType = ft;

    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(func, "entry");
    LLVMPositionBuilderAtEnd(cb->builder, entry);
    
    vs_push(cb->varScope);

    cb->currentFunctionEntry = entry;
    cb->currentFunction = func;
    cb->currentFunctionScope = cb->varScope->scope;

    hashtable param_mapping = hst_create();

    if(a->params)
    {
        int i;
        AST *p = a->params;
        for(i = 0; p; p = p->next, i++)
        {
            EagleTypeType *ty = eparam_types[i];
            LLVMValueRef pos = ac_compile_var_decl(p, cb);

            arr_append(&param_values, pos);
            hst_put(&param_mapping, pos, (void *)(uintptr_t)(i + 1), ahhd, ahed);

            // LLVMBuildStore(cb->builder, LLVMGetParam(func, i), pos);
            if(ET_IS_COUNTED(ty))
                ac_incr_pointer(cb, &pos, eparam_types[i]);
            if(ET_IS_WEAK(ty))
                ac_add_weak_pointer(cb, LLVMGetParam(func, i), pos, eparam_types[i]);
        }
    }

    ac_compile_block(a->body, entry, cb);
    LLVMBuildRet(cb->builder, LLVMConstInt(LLVMInt1Type(), 1, 0));
    // if(!ac_compile_block(a->body, entry, cb) && retType->etype->type != ETVoid)
    //     die(ALN, "Function must return a value.");

    // if(retType->etype->type == ETVoid)
    // {
    //     vs_run_callbacks_through(cb->varScope, cb->varScope->scope);
    //     LLVMBuildRetVoid(cb->builder);
    // }
    vs_pop(cb->varScope);

    char *ctxname = ac_generator_context_name(a->ident);
    LLVMTypeRef ctx = LLVMStructCreateNamed(LLVMGetGlobalContext(), ctxname);
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
    ac_generator_replace_allocas(cb, &gb);
    ac_compile_generator_init(ast, cb, &gb);
    // ac_dump_allocas(cb->currentFunctionEntry, cb);

    // arr_free(&param_indices);
    arr_free(&param_values);
    hst_free(&param_mapping);
    hst_free(&prms);

    cb->currentFunctionEntry = NULL;
}

void ac_generator_replace_allocas(CompilerBundle *cb, GeneratorBundle *gb)
{
    LLVMBasicBlockRef entry = cb->currentFunctionEntry;

    arraylist elems = arr_create(10);
    arr_append(&elems, LLVMPointerType(LLVMInt8Type(), 0));
    arr_append(&elems, LLVMPointerType(LLVMInt8Type(), 0));

    LLVMValueRef first = LLVMGetFirstInstruction(entry);

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
    LLVMValueRef btb = LLVMBuildBitCast(cb->builder, par, LLVMPointerType(ctx, 0), "");
    // LLVMValueRef btb = LLVMBuildAlloca(cb->builder, ctx, "btb");

    // LLVMDumpType(ctx);

    hashtable *pm = gb->param_mapping;
    hashtable *po = gb->params;

    arr_free(&elems);
    elems = arr_create(10);

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
            printf("HERE!\n");
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

    arr_free(&elems);
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

    LLVMTypeRef func_type = LLVMFunctionType(LLVMPointerType(LLVMInt8Type(), 0), param_types, ct, a->vararg);
    LLVMValueRef func = LLVMAddFunction(cb->module, a->ident, func_type);

    vs_put(cb->varScope, a->ident, func, ett_function_type(ett_gen_type(genType->etype), eparam_types, ct));
}

// void ac_compile_generator(AST *ast, CompilerBundle *cb)
// {
//     LLVMValueRef func = NULL;

//     ASTFuncDecl *a = (ASTFuncDecl *)ast;

//     VarBundle *vb = vs_get(cb->varScope, a->ident);
//     func = vb->value;

//     ac_compile_generator_ex(ast, cb, func, (EagleFunctionType *)vb->type);
// }