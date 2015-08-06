#include <stdlib.h>
#include "ast_compiler.h"
#include "variable_manager.h"
#include "core/llvm_headers.h"

typedef struct {
    LLVMModuleRef module;
    LLVMBuilderRef builder;

    EagleFunctionType *currentFunctionType;
    LLVMBasicBlockRef currentFunctionEntry;
    LLVMValueRef currentFunction;
    VarScopeStack *varScope;
} CompilerBundle;

static inline LLVMValueRef ac_build_conversion(LLVMBuilderRef builder, LLVMValueRef val, EagleType from, EagleType to);
static inline LLVMValueRef ac_make_add(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type);
static inline LLVMValueRef ac_make_sub(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type);
static inline LLVMValueRef ac_make_mul(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type);
static inline LLVMValueRef ac_make_div(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type);
static inline LLVMValueRef ac_make_comp(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type, char comp);

LLVMValueRef ac_dispatch_expression(AST *ast, CompilerBundle *cb);
void ac_dispatch_statement(AST *ast, CompilerBundle *cb);
void ac_dispatch_declaration(AST *ast, CompilerBundle *cb);

void ac_compile_function(AST *ast, CompilerBundle *cb);
int ac_compile_block(AST *ast, LLVMBasicBlockRef block, CompilerBundle *cb);
void ac_compile_if(AST *ast, CompilerBundle *cb, LLVMBasicBlockRef mergeBB);
LLVMValueRef ac_compile_function_call(AST *ast, CompilerBundle *cb);

LLVMValueRef ac_compile_value(AST *ast, CompilerBundle *cb)
{
    ASTValue *a = (ASTValue *)ast;
    switch(a->etype)
    {
        case ETInt32:
            a->resultantType = ett_base_type(ETInt32);
            return LLVMConstInt(LLVMInt32Type(), a->value.i, 1);
        case ETDouble:
            a->resultantType = ett_base_type(ETDouble);
            return LLVMConstReal(LLVMDoubleType(), a->value.d);
        default:
            return NULL;
    }
}

LLVMValueRef ac_compile_identifier(AST *ast, CompilerBundle *cb)
{
    ASTValue *a = (ASTValue *)ast;
    VarBundle *b = vs_get(cb->varScope, a->value.id);

    if(!b) // We are dealing with a local variable
    {
        fprintf(stderr, "Error: Undeclared Identifier (%s)\n", a->value.id);
        exit(0);
    }

    if(b->type->type == ETFunction)
    {
        a->resultantType = b->type;
        return b->value;
    }

    a->resultantType = b->type;
    return LLVMBuildLoad(cb->builder, b->value, "loadtmp");
    
    return NULL;
}

LLVMValueRef ac_compile_var_decl(AST *ast, CompilerBundle *cb)
{
    ASTVarDecl *a = (ASTVarDecl *)ast;
    LLVMBasicBlockRef curblock = LLVMGetInsertBlock(cb->builder);
    LLVMPositionBuilderAtEnd(cb->builder, cb->currentFunctionEntry);

    ASTTypeDecl *type = (ASTTypeDecl *)a->atype;

    LLVMValueRef begin = LLVMGetFirstInstruction(cb->currentFunctionEntry);
    if(begin)
        LLVMPositionBuilderBefore(cb->builder, begin);
    LLVMValueRef pos = LLVMBuildAlloca(cb->builder, ett_llvm_type(type->etype), a->ident);
    vs_put(cb->varScope, a->ident, pos, type->etype);

    ast->resultantType = type->etype;

    LLVMPositionBuilderAtEnd(cb->builder, curblock);

    return pos;
}

LLVMValueRef ac_build_store(AST *ast, CompilerBundle *cb)
{
    ASTBinary *a = (ASTBinary *)ast;
    EagleTypeType *totype;
    LLVMValueRef pos;

    if(a->left->type == AIDENT)
    {
        ASTValue *l = (ASTValue *)a->left;
        VarBundle *b = vs_get(cb->varScope, l->value.id);

        totype = b->type;
        pos = b->value;
    }
    else if(a->left->type == AUNARY && ((ASTUnary *)a->left)->op == '*')
    {
        ASTUnary *l = (ASTUnary *)a->left;
        
        pos = ac_dispatch_expression(l->val, cb);
        if(l->val->resultantType->type != ETPointer)
        {
            fprintf(stderr, "Error: Only pointers may be dereferenced.\n");
            exit(1);
        }
        totype = ((EaglePointerType *)l->val->resultantType)->to;
    }
    else
    {
        pos = ac_dispatch_expression(a->left, cb);
        totype = a->left->resultantType;
    }
    /*
    else
    {
        ASTVarDecl *l = (ASTVarDecl *)a->left;
        
        ASTTypeDecl *type = (ASTTypeDecl *)l->atype;
        totype = type->etype;
        pos = ac_compile_var_decl(a->left, cb);
    }
    */

    LLVMValueRef r = ac_dispatch_expression(a->right, cb);
    EagleTypeType *fromtype = a->right->resultantType;

    a->resultantType = totype;

    if(fromtype->type != totype->type)
        r = ac_build_conversion(cb->builder, r, fromtype->type, totype->type);

    LLVMBuildStore(cb->builder, r, pos);
    return LLVMBuildLoad(cb->builder, pos, "loadtmp");
}

LLVMValueRef ac_compile_binary(AST *ast, CompilerBundle *cb)
{
    ASTBinary *a = (ASTBinary *)ast;
    if(a->op == '=')
        return ac_build_store(ast, cb);

    LLVMValueRef l = ac_dispatch_expression(a->left, cb);
    LLVMValueRef r = ac_dispatch_expression(a->right, cb);

    EagleType promo = et_promotion(a->left->resultantType->type, a->right->resultantType->type);
    a->resultantType = ett_base_type(promo);

    if(a->left->resultantType->type != promo)
    {
        l = ac_build_conversion(cb->builder, l, a->left->resultantType->type, promo);
    }
    else if(a->right->resultantType->type != promo)
    {
        r = ac_build_conversion(cb->builder, r, a->right->resultantType->type, promo);
    }

    switch(a->op)
    {
        case '+':
            return ac_make_add(l, r, cb->builder, promo);
        case '-':
            return ac_make_sub(l, r, cb->builder, promo);
        case '*':
            return ac_make_mul(l, r, cb->builder, promo);
        case '/':
            return ac_make_div(l, r, cb->builder, promo);
        case 'e':
        case 'n':
        case 'g':
        case 'l':
        case 'G':
        case 'L':
            return ac_make_comp(l, r, cb->builder, promo, a->op);
        default:
            return NULL;
    }
}

LLVMValueRef ac_compile_get_address(AST *of, CompilerBundle *cb)
{
    ASTValue *o = (ASTValue *)of;
    VarBundle *b = vs_get(cb->varScope, o->value.id);

    if(!b)
    {
        fprintf(stderr, "Error: Undeclared identifier (%s)\n", o->value.id);
        exit(0);
    }
    
    of->resultantType = b->type;

    return b->value;
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

    LLVMValueRef v = ac_dispatch_expression(a->val, cb);

    switch(a->op)
    {
        case 'p':
            {
                LLVMValueRef fmt = NULL;
                switch(a->val->resultantType->type)
                {
                    case ETDouble:
                        fmt = LLVMBuildGlobalStringPtr(cb->builder, "%lf\n", "prfLF");
                        break;
                    case ETInt32:
                        fmt = LLVMBuildGlobalStringPtr(cb->builder, "%d\n", "prfI");
                        break;
                    case ETInt64:
                        fmt = LLVMBuildGlobalStringPtr(cb->builder, "%ld\n", "prfLI");
                        break;
                    case ETPointer:
                        fmt = LLVMBuildGlobalStringPtr(cb->builder, "%p\n", "prfPTR");
                        break;
                    default:
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
                {
                    fprintf(stderr, "Error: Only pointers may be dereferenced.\n");
                    exit(1);
                }

                LLVMValueRef r = LLVMBuildLoad(cb->builder, v, "dereftmp");
                EaglePointerType *pt = (EaglePointerType *)a->val->resultantType;
                a->resultantType = pt->to;
                return r;
            }
        case '!':
            // TODO: Broken
            return LLVMBuildNot(cb->builder, v, "nottmp");
        default:
            break;
    }

    return NULL;
}

LLVMValueRef ac_dispatch_expression(AST *ast, CompilerBundle *cb)
{
    switch(ast->type)
    {
        case AVALUE:
            return ac_compile_value(ast, cb);
        case ABINARY:
            return ac_compile_binary(ast, cb);
        case AUNARY:
            return ac_compile_unary(ast, cb);
        case AVARDECL:
            return ac_compile_var_decl(ast, cb);
        case AIDENT:
            return ac_compile_identifier(ast, cb);
        case AFUNCCALL:
            return ac_compile_function_call(ast, cb);
        default:
            return NULL;
    }
}

void ac_dispatch_statement(AST *ast, CompilerBundle *cb)
{
    switch(ast->type)
    {
        case AVALUE:
            ac_compile_value(ast, cb);
            break;
        case ABINARY:
            ac_compile_binary(ast, cb);
            break;
        case AUNARY:
            ac_compile_unary(ast, cb);
            break;
        case AVARDECL:
            ac_compile_var_decl(ast, cb);
            break;
        case AIDENT:
            ac_compile_identifier(ast, cb);
            break;
        case AFUNCCALL:
            ac_compile_function_call(ast, cb);
            break;
        case AIF:
            ac_compile_if(ast, cb, NULL);
            break;
        default:
            return;
    }
}

void ac_dispatch_declaration(AST *ast, CompilerBundle *cb)
{
    switch(ast->type)
    {
        case AFUNCDECL:
            ac_compile_function(ast, cb);
            break;
        default:
            return;
    }
}

void ac_compile_if(AST *ast, CompilerBundle *cb, LLVMBasicBlockRef mergeBB)
{
    ASTIfBlock *a = (ASTIfBlock *)ast;
    LLVMValueRef val = ac_dispatch_expression(a->test, cb);

    LLVMValueRef cmp = NULL;
    if(a->test->resultantType->type == ETInt32)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntNE, val, LLVMConstInt(LLVMInt32Type(), 0, 0), "cmp");
    else if(a->test->resultantType->type == ETInt64)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntNE, val, LLVMConstInt(LLVMInt64Type(), 0, 0), "cmp");
    else
        cmp = LLVMBuildFCmp(cb->builder, LLVMRealONE, val, LLVMConstReal(LLVMDoubleType(), 0.0), "cmp");

    int multiBlock = a->ifNext && ((ASTIfBlock *)a->ifNext)->test;
    int threeBlock = !!a->ifNext && !multiBlock;

    LLVMBasicBlockRef ifBB = LLVMAppendBasicBlock(cb->currentFunction, "if");
    LLVMBasicBlockRef elseBB = threeBlock || multiBlock ? LLVMAppendBasicBlock(cb->currentFunction, "else") : NULL;
    if(!mergeBB)
        mergeBB = LLVMAppendBasicBlock(cb->currentFunction, "merge");

    // WARNING: Broken code.
    // TODO: This needs to be replaced with a formal nested if statement
    LLVMBuildCondBr(cb->builder, cmp, ifBB, elseBB ? elseBB : mergeBB);
    LLVMPositionBuilderAtEnd(cb->builder, ifBB);

    vs_push(cb->varScope);
    if(!ac_compile_block(a->block, ifBB, cb))
    {
        LLVMBuildBr(cb->builder, mergeBB);
    }
    vs_pop(cb->varScope);

    if(threeBlock)
    {
        ASTIfBlock *el = (ASTIfBlock *)a->ifNext;
        LLVMPositionBuilderAtEnd(cb->builder, elseBB);

        vs_push(cb->varScope);
        if(!ac_compile_block(el->block, elseBB, cb))
            LLVMBuildBr(cb->builder, mergeBB);
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

int ac_compile_block(AST *ast, LLVMBasicBlockRef block, CompilerBundle *cb)
{
    for(; ast; ast = ast->next)
    {
        if(ast->type == AUNARY && ((ASTUnary *)ast)->op == 'r') // Handle the special return case
        {
            LLVMValueRef val = ac_dispatch_expression(((ASTUnary *)ast)->val, cb);
            EagleType t = ((ASTUnary *)ast)->val->resultantType->type;
            EagleType o = cb->currentFunctionType->retType->type;

            if(t != o)
                val = ac_build_conversion(cb->builder, val, t, o);

            LLVMBuildRet(cb->builder, val);

            return 1;
        }
        
        ac_dispatch_statement(ast, cb);
    }

    return 0;
}

LLVMValueRef ac_compile_function_call(AST *ast, CompilerBundle *cb)
{
    ASTFuncCall *a = (ASTFuncCall *)ast;

    LLVMValueRef func = ac_dispatch_expression(a->callee, cb);
    
    EagleFunctionType *ett = (EagleFunctionType *)a->callee->resultantType;

    a->resultantType = ett->retType;

    AST *p;
    int i;
    for(p = a->params, i = 0; p; p = p->next, i++);
    int ct = i;

    LLVMValueRef args[ct];
    for(p = a->params, i = 0; p; p = p->next, i++)
    {
        LLVMValueRef val = ac_dispatch_expression(p, cb);
        EagleType rt = p->resultantType->type;
        if(rt != ett->params[i]->type)
            val = ac_build_conversion(cb->builder, val, rt, ett->params[i]->type);
        args[i] = val;
    }

    return LLVMBuildCall(cb->builder, func, args, ct, ett->retType->type == ETVoid ? "" : "callout");
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

    if(a->params)
    {
        int i;
        AST *p = a->params;
        for(i = 0; p; p = p->next, i++)
        {
            LLVMValueRef pos = ac_compile_var_decl(p, cb);
            LLVMBuildStore(cb->builder, LLVMGetParam(func, i), pos);
        }
    }

    ac_compile_block(a->body, entry, cb);

    if(retType->etype->type == ETVoid)
        LLVMBuildRetVoid(cb->builder);

    cb->currentFunctionEntry = NULL;

    vs_pop(cb->varScope);
}

void ac_prepare_module(LLVMModuleRef module)
{
    LLVMTypeRef param_types[] = {LLVMPointerType(LLVMInt8Type(), 0)};
    LLVMTypeRef func_type = LLVMFunctionType(LLVMInt32Type(), param_types, 1, 1);
    LLVMAddFunction(module, "printf", func_type);
}

void ac_add_early_declarations(AST *ast, CompilerBundle *cb)
{
    if(ast->type != AFUNCDECL)
        return;

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
    }

    ASTTypeDecl *retType = (ASTTypeDecl *)a->retType;
    LLVMTypeRef func_type = LLVMFunctionType(ett_llvm_type(retType->etype), param_types, ct, 0);
    LLVMValueRef func = LLVMAddFunction(cb->module, a->ident, func_type);

    vs_put(cb->varScope, a->ident, func, ett_function_type(retType->etype, eparam_types, ct));
}

LLVMModuleRef ac_compile(AST *ast)
{
    CompilerBundle cb;
    cb.module = LLVMModuleCreateWithName("main-module");
    cb.builder = LLVMCreateBuilder();
    
    VarScopeStack vs = vs_make();
    cb.varScope = &vs;

    vs_push(cb.varScope);

    ac_prepare_module(cb.module);

    AST *old = ast;
    for(; ast; ast = ast->next)
        ac_add_early_declarations(ast, &cb);

    ast = old;

    for(; ast; ast = ast->next)
    {
        ac_dispatch_declaration(ast, &cb);
    }

    vs_pop(cb.varScope);

    LLVMDisposeBuilder(cb.builder);
    vs_free(cb.varScope);
    return cb.module;
}

LLVMValueRef ac_build_conversion(LLVMBuilderRef builder, LLVMValueRef val, EagleType from, EagleType to)
{
    switch(from)
    {
        case ETInt32:
            switch(to)
            {
                case ETInt32:
                    return val;
                case ETInt64:
                    return LLVMBuildIntCast(builder, val, LLVMInt64Type(), "conv");
                case ETDouble:
                    return LLVMBuildSIToFP(builder, val, LLVMDoubleType(), "conv");
                default:
                    break;
            }
            break;
        case ETInt64:
            switch(to)
            {
                case ETInt64:
                    return val;
                case ETInt32:
                    return LLVMBuildIntCast(builder, val, LLVMInt32Type(), "conv");
                case ETDouble:
                    return LLVMBuildSIToFP(builder, val, LLVMDoubleType(), "conv");
                default:
                    break;
            }
            break;
        case ETDouble:
            switch(to)
            {
                case ETDouble:
                    return val;
                case ETInt32:
                    return LLVMBuildFPToSI(builder, val, LLVMInt32Type(), "conv");
                case ETInt64:
                    return LLVMBuildFPToSI(builder, val, LLVMInt64Type(), "conv");
                default:
                    break;
            }
            break;
        default:
            break;
    }

    return NULL;
}

LLVMValueRef ac_make_add(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type)
{
    switch(type)
    {
        case ETDouble:
            return LLVMBuildFAdd(builder, left, right, "addtmp");
        case ETInt32:
        case ETInt64:
            return LLVMBuildAdd(builder, left, right, "addtmp");
        default:
            return NULL;
    }
}

LLVMValueRef ac_make_sub(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type)
{
    switch(type)
    {
        case ETDouble:
            return LLVMBuildFSub(builder, left, right, "subtmp");
        case ETInt32:
        case ETInt64:
            return LLVMBuildSub(builder, left, right, "subtmp");
        default:
            return NULL;
    }
}

LLVMValueRef ac_make_mul(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type)
{
    switch(type)
    {
        case ETDouble:
            return LLVMBuildFMul(builder, left, right, "multmp");
        case ETInt32:
        case ETInt64:
            return LLVMBuildMul(builder, left, right, "multmp");
        default:
            return NULL;
    }
}

LLVMValueRef ac_make_div(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type)
{
    switch(type)
    {
        case ETDouble:
            return LLVMBuildFDiv(builder, left, right, "divtmp");
        case ETInt32:
        case ETInt64:
            return LLVMBuildSDiv(builder, left, right, "divtmp");
        default:
            return NULL;
    }
}

LLVMValueRef ac_make_comp(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type, char comp)
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
        case ETDouble:
            return LLVMBuildFCmp(builder, rp, left, right, "eqtmp");
        case ETInt32:
        case ETInt64:
            return LLVMBuildICmp(builder, ip, left, right, "eqtmp");
        default:
            return NULL;
    }
}

