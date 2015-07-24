#include "ast_compiler.h"
#include "variable_manager.h"
#include "core/llvm_headers.h"

typedef struct {
    LLVMModuleRef module;
    LLVMBuilderRef builder;

    EagleType currentFunctionReturnType;
    LLVMBasicBlockRef currentFunctionEntry;
    VarScope *varScope;
} CompilerBundle;

static inline LLVMValueRef ac_build_conversion(LLVMBuilderRef builder, LLVMValueRef val, EagleType from, EagleType to);
static inline LLVMValueRef ac_make_add(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type);

LLVMValueRef ac_dispatch_expression(AST *ast, CompilerBundle *cb);
void ac_dispatch_statement(AST *ast, CompilerBundle *cb);
void ac_dispatch_declaration(AST *ast, CompilerBundle *cb);

void ac_compile_function(AST *ast, CompilerBundle *cb);
int ac_compile_block(AST *ast, LLVMBasicBlockRef block, CompilerBundle *cb);

LLVMValueRef ac_compile_value(AST *ast, CompilerBundle *cb)
{
    ASTValue *a = (ASTValue *)ast;
    switch(a->etype)
    {
        case ETInt32:
            a->resultantType = ETInt32;
            return LLVMConstInt(LLVMInt32Type(), a->value.i, 1);
        case ETDouble:
            a->resultantType = ETDouble;
            return LLVMConstReal(LLVMDoubleType(), a->value.d);
        default:
            return NULL;
    }
}

LLVMValueRef ac_compile_identifier(AST *ast, CompilerBundle *cb)
{
    ASTValue *a = (ASTValue *)ast;
    VarBundle *b = vs_get(cb->varScope, a->value.id);

    a->resultantType = b->type;
    return LLVMBuildLoad(cb->builder, b->value, "loadtmp");
}

LLVMValueRef ac_compile_var_decl(AST *ast, CompilerBundle *cb)
{
    ASTVarDecl *a = (ASTVarDecl *)ast;
    LLVMBasicBlockRef curblock = LLVMGetInsertBlock(cb->builder);
    LLVMPositionBuilderAtEnd(cb->builder, cb->currentFunctionEntry);

    LLVMValueRef pos = LLVMBuildAlloca(cb->builder, et_llvm_type(a->etype), a->ident);
    vs_put(cb->varScope, a->ident, pos, a->etype);

    LLVMPositionBuilderAtEnd(cb->builder, curblock);

    return pos;
}

LLVMValueRef ac_make_add(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type)
{
    switch(type)
    {
        case ETDouble:
            return LLVMBuildFAdd(builder, left, right, "addtmp");
        case ETInt32:
            return LLVMBuildAdd(builder, left, right, "addtmp");
        default:
            return NULL;
    }
}

LLVMValueRef ac_build_store(AST *ast, CompilerBundle *cb)
{
    ASTBinary *a = (ASTBinary *)ast;
    LLVMValueRef pos;
    EagleType totype;

    if(a->left->type == AIDENT)
    {
        ASTValue *l = (ASTValue *)a->left;
        VarBundle *b = vs_get(cb->varScope, l->value.id);

        totype = b->type;
        pos = b->value;
    }
    else
    {
        ASTVarDecl *l = (ASTVarDecl *)a->left;
        
        totype = l->etype;
        pos = ac_compile_var_decl(a->left, cb);
    }

    LLVMValueRef r = ac_dispatch_expression(a->right, cb);
    EagleType fromtype = a->right->resultantType;

    a->resultantType = totype;

    if(fromtype != totype)
        r = ac_build_conversion(cb->builder, r, fromtype, totype);

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

    EagleType promo = et_promotion(a->left->resultantType, a->right->resultantType);
    a->resultantType = promo;

    if(a->left->resultantType != promo)
    {
        l = ac_build_conversion(cb->builder, l, a->left->resultantType, promo);
    }
    else if(a->right->resultantType != promo)
    {
        r = ac_build_conversion(cb->builder, r, a->right->resultantType, promo);
    }

    switch(a->op)
    {
        case '+':
            return ac_make_add(l, r, cb->builder, promo);
        default:
            return NULL;
    }
}

LLVMValueRef ac_compile_unary(AST *ast, CompilerBundle *cb)
{
    ASTUnary *a = (ASTUnary *)ast;
    LLVMValueRef v = ac_dispatch_expression(a->val, cb);

    switch(a->op)
    {
        case 'p':
            {
                LLVMValueRef fmt = NULL;
                switch(a->val->resultantType)
                {
                    case ETDouble:
                        fmt = LLVMBuildGlobalStringPtr(cb->builder, "%lf\n", "prfLF");
                        break;
                    case ETInt32:
                        fmt = LLVMBuildGlobalStringPtr(cb->builder, "%d\n", "prfI");
                        break;
                    default:
                        break;
                }

                LLVMValueRef func = LLVMGetNamedFunction(cb->module, "printf");
                LLVMValueRef args[] = {fmt, v};
                LLVMBuildCall(cb->builder, func, args, 2, "putsout");
                return NULL;
            }
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
        case AVARDECL:
            return ac_compile_var_decl(ast, cb);
        case AIDENT:
            return ac_compile_identifier(ast, cb);
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

int ac_compile_block(AST *ast, LLVMBasicBlockRef block, CompilerBundle *cb)
{
    for(; ast; ast = ast->next)
    {
        if(ast->type == AUNARY && ((ASTUnary *)ast)->op == 'r') // Handle the special return case
        {
            LLVMValueRef val = ac_dispatch_expression(((ASTUnary *)ast)->val, cb);
            EagleType t = ((ASTUnary *)ast)->val->resultantType;
            EagleType o = cb->currentFunctionReturnType;

            printf("%d %d\n", t, o);

            if(t != o)
                val = ac_build_conversion(cb->builder, val, t, o);

            LLVMBuildRet(cb->builder, val);
            return 1;
        }
        
        ac_dispatch_statement(ast, cb);
    }

    return 0;
}

void ac_compile_function(AST *ast, CompilerBundle *cb)
{
    ASTFuncDecl *a = (ASTFuncDecl *)ast;
    
    int i;
    AST *p = a->params;
    for(i = 0; p; p = p->next, i++);

    int ct = i;

    LLVMTypeRef param_types[ct];
    for(i = 0, p = a->params; i < ct; p = p->next, i++)
    {
        param_types[i] = et_llvm_type(((ASTVarDecl *)p)->etype);
    }

    LLVMTypeRef func_type = LLVMFunctionType(et_llvm_type(a->retType), param_types, ct, 0);
    LLVMValueRef func = LLVMAddFunction(cb->module, a->ident, func_type);

    cb->currentFunctionReturnType = a->retType;

    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(func, "entry");
    LLVMPositionBuilderAtEnd(cb->builder, entry);

    VarScope scope = vs_make();

    cb->currentFunctionEntry = entry;
    cb->varScope = &scope;

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

    if(!ac_compile_block(a->body, entry, cb))
        LLVMBuildRetVoid(cb->builder);

    cb->currentFunctionEntry = NULL;
    cb->varScope = NULL;
    vs_free(&scope);
}

void ac_prepare_module(LLVMModuleRef module)
{
    LLVMTypeRef param_types[] = {LLVMPointerType(LLVMInt8Type(), 0)};
    LLVMTypeRef func_type = LLVMFunctionType(LLVMInt32Type(), param_types, 1, 1);
    LLVMAddFunction(module, "printf", func_type);
}

LLVMModuleRef ac_compile(AST *ast)
{
    CompilerBundle cb;
    cb.module = LLVMModuleCreateWithName("main-module");
    cb.builder = LLVMCreateBuilder();

    ac_prepare_module(cb.module);

    for(; ast; ast = ast->next)
    {
        ac_dispatch_declaration(ast, &cb);
    }

    LLVMDisposeBuilder(cb.builder);
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
                default:
                    break;
            }
            break;
        default:
            break;
    }

    return NULL;
}

