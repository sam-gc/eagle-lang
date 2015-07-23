#include "ast_compiler.h"
#include "core/llvm_headers.h"

typedef struct {
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    EagleType currentFunctionReturnType;
} CompilerBundle;

inline LLVMValueRef ac_build_conversion(LLVMBuilderRef builder, LLVMValueRef val, EagleType from, EagleType to);

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

inline LLVMValueRef ac_make_add(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type)
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

LLVMValueRef ac_compile_binary(AST *ast, CompilerBundle *cb)
{
    ASTBinary *a = (ASTBinary *)ast;
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

/*
LLVMValueRef ac_compile_unary(AST *ast, CompilerBundle *cb)
{
    ASTUnary *a = (ASTUnary *)ast;
    LLVMValueRef v = ac_dispatch_expression(a->val, cb);

    retur
}
*/

LLVMValueRef ac_dispatch_expression(AST *ast, CompilerBundle *cb)
{
    switch(ast->type)
    {
        case AVALUE:
            return ac_compile_value(ast, cb);
        case ABINARY:
            return ac_compile_binary(ast, cb);
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
    LLVMTypeRef param_types[] = {};
    LLVMTypeRef func_type = LLVMFunctionType(et_llvm_type(a->retType), param_types, 0, 0);
    LLVMValueRef func = LLVMAddFunction(cb->module, a->ident, func_type);

    cb->currentFunctionReturnType = a->retType;

    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(func, "entry");
    LLVMPositionBuilderAtEnd(cb->builder, entry);

    if(!ac_compile_block(a->body, entry, cb))
        LLVMBuildRetVoid(cb->builder);
}

LLVMModuleRef ac_compile(AST *ast)
{
    CompilerBundle cb;
    cb.module = LLVMModuleCreateWithName("main-module");
    cb.builder = LLVMCreateBuilder();

    for(; ast; ast = ast->next)
    {
        ac_dispatch_declaration(ast, &cb);
    }

    LLVMDisposeBuilder(cb.builder);
    return cb.module;
}

inline LLVMValueRef ac_build_conversion(LLVMBuilderRef builder, LLVMValueRef val, EagleType from, EagleType to)
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

