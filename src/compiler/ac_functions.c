#include "ast_compiler.h"

char *ac_closure_context_name(char *name)
{
    char *str = malloc(strlen(name) + 11);
    sprintf(str, "__egl_ctx_%s", name);
    return str;
}

char *ac_closure_code_name(char *name)
{
    char *str = malloc(strlen(name) + 12);
    sprintf(str, "__egl_code_%s", name);
    return str;
}

char *ac_closure_closure_name(char *name)
{
    char *str = malloc(strlen(name) + 11);
    sprintf(str, "__egl_clj_%s", name);
    return str;
}

char *ac_closure_destructor_name(char *name)
{
    char *str = malloc(strlen(name) + 10);
    sprintf(str, "__egl_cx_%s", name);
    return str;
}

void ac_pre_prepare_closure(CompilerBundle *cb, char *name, ClosureBundle *bun)
{
    bun->name = name;
    bun->cb = cb;
    bun->cfib = LLVMGetInsertBlock(cb->builder);

    bun->outerContext = NULL;
    bun->context = NULL;
}

LLVMValueRef ac_finish_closure(CompilerBundle *cb, ClosureBundle *bun, LLVMTypeRef *storageType)
{
    char *cloname = ac_closure_closure_name(bun->name);
    LLVMTypeRef cloType = LLVMStructCreateNamed(LLVMGetGlobalContext(), cloname);
    free(cloname);

    bun->contextType = NULL;
    if(bun->contextTypes->count)
    {
        char *ctxname = ac_closure_context_name(bun->name);
        bun->contextType = LLVMStructCreateNamed(LLVMGetGlobalContext(), ctxname);
        free(ctxname);
        LLVMStructSetBody(bun->contextType, (LLVMTypeRef *)bun->contextTypes->items, bun->contextTypes->count, 0);
    }

    LLVMTypeRef strTys[2];

    strTys[0] = LLVMPointerType(bun->funcType, 0);
    strTys[1] = LLVMPointerType(LLVMInt8Type(), 0);

    LLVMStructSetBody(cloType, strTys, 2, 0);

    LLVMPositionBuilderAtEnd(cb->builder, bun->cfib);

    LLVMTypeRef ultType;
    LLVMValueRef countedFunc = ac_compile_malloc_counted_raw(cloType, &ultType, cb);

    LLVMValueRef contextTypeStruct = NULL;
    if(bun->contextTypes->count > 0)
        contextTypeStruct = LLVMBuildMalloc(cb->builder, bun->contextType, "");

    LLVMValueRef theFunc = LLVMBuildStructGEP(cb->builder, countedFunc, 5, "");
    *storageType = ultType;

    LLVMValueRef pos = LLVMBuildStructGEP(cb->builder, theFunc, 0, "");
    LLVMBuildStore(cb->builder, bun->function, pos);

    if(bun->contextTypes->count)
    {
        bun->outerContext = LLVMBuildStructGEP(cb->builder, theFunc, 1, "blockContext");

        LLVMBuildStore(cb->builder, LLVMBuildBitCast(cb->builder, contextTypeStruct, LLVMPointerType(LLVMInt8Type(), 0), ""), bun->outerContext);
        // bun->outerContext = LLVMBuildBitCast(cb->builder, , bun->contextType, "");


        // LLVMValueRef checkPos = LLVMBuildStructGEP(cb->builder, theFunc, 1, "");
        // LLVMBuildStore(cb->builder, LLVMConstInt(LLVMInt1Type(), 1, 0), checkPos);

        int i;
        for(i = 0; i < bun->outerContextVals->count; i++)
        {
            VarBundle *o = bun->outerContextVals->items[i];

            LLVMValueRef pos = LLVMBuildStructGEP(cb->builder, contextTypeStruct, i, "");
            LLVMBuildStore(cb->builder, LLVMBuildLoad(cb->builder, o->value, ""), pos);
            ac_incr_pointer(cb, &pos, o->type);
        }

        LLVMValueRef first = LLVMGetFirstInstruction(bun->entry);
        LLVMPositionBuilderBefore(cb->builder, first);

        LLVMValueRef tmp = LLVMGetParam(bun->function, 0);
        bun->context = LLVMBuildBitCast(cb->builder, tmp, LLVMPointerType(bun->contextType, 0), "context");

        for(i = 0; i < bun->contextVals->count; i++)
        {
            LLVMValueRef temp = bun->contextVals->items[i];
            LLVMValueRef var = LLVMBuildStructGEP(cb->builder, bun->context, i, "");

            LLVMReplaceAllUsesWith(temp, var);
        }
        for(i = 0; i < bun->contextVals->count; i++)
        {
            LLVMValueRef temp = bun->contextVals->items[i];
            LLVMInstructionEraseFromParent(temp);
        }

        LLVMTypeRef des_params[2];
        des_params[0] = LLVMPointerType(LLVMInt8Type(), 0);
        des_params[1] = LLVMInt1Type();

        char *desname = ac_closure_destructor_name(bun->name);
        LLVMTypeRef des_func = LLVMFunctionType(LLVMVoidType(), des_params, 2, 0);
        LLVMValueRef func_des = LLVMAddFunction(cb->module, desname, des_func);
        free(desname);

        LLVMBasicBlockRef dentry = LLVMAppendBasicBlock(func_des, "entry");

        LLVMPositionBuilderAtEnd(cb->builder, dentry);
        LLVMValueRef strct = LLVMBuildBitCast(cb->builder, LLVMGetParam(func_des, 0), LLVMPointerType(ultType, 0), "");
        strct = LLVMBuildStructGEP(cb->builder, strct, 5, "");

        strct = LLVMBuildStructGEP(cb->builder, strct, 1, "");
        strct = LLVMBuildLoad(cb->builder, strct, "");

        strct = LLVMBuildBitCast(cb->builder, strct, LLVMPointerType(bun->contextType, 0), "");

        for(i = 0; i < bun->contextTypes->count; i++)
        {
            LLVMValueRef pos = LLVMBuildStructGEP(cb->builder, strct, i, "");
            ac_decr_pointer(cb, &pos, NULL);
        }

        LLVMBuildFree(cb->builder, strct);

        LLVMBuildRetVoid(cb->builder);

        LLVMPositionBuilderAtEnd(cb->builder, bun->cfib);
        LLVMValueRef pos = LLVMBuildStructGEP(cb->builder, countedFunc, 4, "");
        LLVMBuildStore(cb->builder, func_des, pos);
    }
    else
    {
        LLVMValueRef ctxPos = LLVMBuildStructGEP(cb->builder, theFunc, 1, "");
        LLVMBuildStore(cb->builder, LLVMConstPointerNull(LLVMPointerType(LLVMInt8Type(), 0)), ctxPos);
    }

    LLVMPositionBuilderAtEnd(cb->builder, bun->cfib);

    return countedFunc;
}

void ac_closure_callback(VarBundle *vb, char *ident, void *data)
{
    ClosureBundle *bun = data;
    CompilerBundle *cb = bun->cb;

    if(!ET_IS_CLOSED(vb->type))
        ac_replace_with_counted(cb, vb);

    arr_append(bun->contextTypes, ett_llvm_type(vb->type));

    LLVMBasicBlockRef curPos = LLVMGetInsertBlock(cb->builder);
    LLVMPositionBuilderAtEnd(cb->builder, bun->cfib);

    arr_append(bun->outerContextVals, vb);

    LLVMPositionBuilderAtEnd(cb->builder, bun->entry);

    LLVMValueRef temp = LLVMBuildAlloca(cb->builder, ett_llvm_type(vb->type), "TEMP___TEMP");
    vs_put(cb->varScope, ident, temp, vb->type);
    arr_append(bun->contextVals, temp);

    LLVMPositionBuilderAtEnd(cb->builder, curPos);
}

LLVMValueRef ac_compile_closure(AST *ast, CompilerBundle *cb)
{
    ASTFuncDecl *a = (ASTFuncDecl *)ast;

    EagleFunctionType *l_ftype = cb->currentFunctionType;
    LLVMValueRef l_cf = cb->currentFunction;
    LLVMBasicBlockRef l_entry = cb->currentFunctionEntry;
    VarScope *l_scope = cb->currentFunctionScope;

    ClosureBundle cloclo;
    ac_pre_prepare_closure(cb, a->ident, &cloclo);

    arraylist list = arr_create(8);
    arraylist l2 = arr_create(8);
    arraylist l3 = arr_create(8);
    cloclo.contextTypes = &list;
    cloclo.contextVals = &l2;
    cloclo.outerContextVals = &l3;

    char *ccode = ac_closure_code_name(a->ident);

    int i;
    AST *p = a->params;
    for(i = 0; p; p = p->next, i++);

    int ct = i + 1;
    EagleTypeType *eparam_types[ct];
    LLVMTypeRef param_types[ct];
    for(i = 1, p = a->params; i < ct; p = p->next, i++)
    {
        ASTTypeDecl *type = (ASTTypeDecl *)((ASTVarDecl *)p)->atype;
        eparam_types[i] = type->etype;
        param_types[i] = ett_llvm_type(type->etype);
    }
    eparam_types[0] = ett_pointer_type(ett_base_type(ETInt8));
    param_types[0] = LLVMPointerType(LLVMInt8Type(), 0);

    ASTTypeDecl *retType = (ASTTypeDecl *)a->retType;

    EagleTypeType *ultimateEType = ett_function_type(retType->etype, eparam_types + 1, ct - 1);

    LLVMTypeRef funcType = LLVMFunctionType(ett_llvm_type(retType->etype), param_types, ct, 0);

    LLVMValueRef func = LLVMAddFunction(cb->module, ccode, funcType);
    cloclo.funcType = funcType;
    
    cloclo.function = func;

    cb->currentFunctionType = (EagleFunctionType *)ett_function_type(retType->etype, eparam_types, ct);

    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(func, "entry");
    LLVMPositionBuilderAtEnd(cb->builder, entry);
    
    vs_push_closure(cb->varScope, ac_closure_callback, &cloclo);
    vs_push(cb->varScope);

    cb->currentFunctionEntry = entry;
    cb->currentFunction = func;
    cb->currentFunctionScope = cb->varScope->scope;

    cloclo.entry = entry;

    if(a->params)
    {
        int i;
        AST *p = a->params;
        for(i = 1; p; p = p->next, i++)
        {
            EagleTypeType *ty = eparam_types[i];
            LLVMValueRef pos = ac_compile_var_decl(p, cb);
            LLVMBuildStore(cb->builder, LLVMGetParam(func, i), pos);
            if(ET_IS_COUNTED(ty))
                ac_incr_pointer(cb, &pos, eparam_types[i]);
            if(ET_IS_WEAK(ty))
                ac_add_weak_pointer(cb, LLVMGetParam(func, i), pos, eparam_types[i]);
        }
    }

    ((EagleFunctionType *)ultimateEType)->closure = CLOSURE_RECURSE;
    EagleTypeType *penultEType = ultimateEType;
    LLVMValueRef pos = LLVMBuildAlloca(cb->builder, ett_llvm_type(penultEType), "recur");

    LLVMValueRef posa = LLVMBuildStructGEP(cb->builder, pos, 0, "");
    LLVMValueRef conv = LLVMBuildBitCast(cb->builder, LLVMGetNamedFunction(cb->module, ccode), LLVMPointerType(LLVMInt8Type(), 0), "");
    LLVMBuildStore(cb->builder, conv, posa);

    LLVMValueRef posb = LLVMBuildStructGEP(cb->builder, pos, 1, "");
    conv = LLVMBuildBitCast(cb->builder, LLVMGetParam(func, 0), LLVMPointerType(LLVMInt8Type(), 0), "");
    LLVMBuildStore(cb->builder, conv, posb);

    vs_put(cb->varScope, "recur", pos, penultEType);

    if(!ac_compile_block(a->body, entry, cb) && retType->etype->type != ETVoid)
        die(ALN, "Function must return a value.");

    if(retType->etype->type == ETVoid)
    {
        vs_run_callbacks_through(cb->varScope, cb->varScope->scope);
        LLVMBuildRetVoid(cb->builder);
    }

    vs_pop(cb->varScope);
    vs_pop(cb->varScope);

    LLVMTypeRef ultType = NULL;
    LLVMValueRef built = ac_finish_closure(cb, &cloclo, &ultType);
    cb->currentFunctionEntry = NULL;

    ((EagleFunctionType *)ultimateEType)->closure = list.count == 0 ? CLOSURE_NO_CLOSE : CLOSURE_CLOSE;

    arr_free(&list);
    arr_free(&l2);
    arr_free(&l3);
    free(ccode);

    cb->currentFunctionType = l_ftype;
    cb->currentFunction = l_cf;
    cb->currentFunctionEntry = l_entry;
    cb->currentFunctionScope = l_scope;

    LLVMPositionBuilderAtEnd(cb->builder, cloclo.cfib);

    ultimateEType = ett_pointer_type(ultimateEType);
    ((EaglePointerType *)ultimateEType)->counted = 1;

    /*
    LLVMValueRef out = LLVMBuildAlloca(cb->builder, LLVMPointerType(ultType, 0), "");
    LLVMBuildStore(cb->builder, built, out);
    vs_put(cb->varScope, a->ident, out, ultimateEType);
    vs_add_callback(cb->varScope, a->ident, ac_scope_leave_callback, cb);
    */

    ast->resultantType = ultimateEType;
    hst_put(&cb->transients, ast, built, ahhd, ahed);

    return LLVMBuildBitCast(cb->builder, built, ett_llvm_type(ultimateEType), "");
}

void ac_compile_function(AST *ast, CompilerBundle *cb)
{
    ASTFuncDecl *a = (ASTFuncDecl *)ast;

    if(!a->body) // This is an extern definition
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

    ASTTypeDecl *retType = (ASTTypeDecl *)a->retType;

    LLVMValueRef func = NULL;

    VarBundle *vb = vs_get(cb->varScope, a->ident);
    func = vb->value;
    cb->currentFunctionType = (EagleFunctionType *)vb->type;

    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(func, "entry");
    LLVMPositionBuilderAtEnd(cb->builder, entry);
    
    vs_push(cb->varScope);

    cb->currentFunctionEntry = entry;
    cb->currentFunction = func;
    cb->currentFunctionScope = cb->varScope->scope;

    if(a->params)
    {
        int i;
        AST *p = a->params;
        for(i = 0; p; p = p->next, i++)
        {
            EagleTypeType *ty = eparam_types[i];
            LLVMValueRef pos = ac_compile_var_decl(p, cb);
            LLVMBuildStore(cb->builder, LLVMGetParam(func, i), pos);
            if(ET_IS_COUNTED(ty))
                ac_incr_pointer(cb, &pos, eparam_types[i]);
            if(ET_IS_WEAK(ty))
                ac_add_weak_pointer(cb, LLVMGetParam(func, i), pos, eparam_types[i]);
        }
    }

    if(!ac_compile_block(a->body, entry, cb) && retType->etype->type != ETVoid)
        die(ALN, "Function must return a value.");

    if(retType->etype->type == ETVoid)
    {
        vs_run_callbacks_through(cb->varScope, cb->varScope->scope);
        LLVMBuildRetVoid(cb->builder);
    }
    vs_pop(cb->varScope);

    cb->currentFunctionEntry = NULL;
}
