#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "ast_compiler.h"
#include "variable_manager.h"
#include "core/llvm_headers.h"

#define IS_ANY_PTR(t) (t->type == ETPointer && ((EaglePointerType *)t)->to->type == ETAny)
#define ALN (ast->lineno)
#define LN(a) (a->lineno)

typedef struct {
    LLVMModuleRef module;
    LLVMBuilderRef builder;

    EagleFunctionType *currentFunctionType;
    LLVMBasicBlockRef currentFunctionEntry;
    LLVMValueRef currentFunction;
    VarScopeStack *varScope;
} CompilerBundle;

static inline LLVMValueRef ac_build_conversion(LLVMBuilderRef builder, LLVMValueRef val, EagleTypeType *from, EagleTypeType *to);
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
LLVMValueRef ac_compile_index(AST *ast, int keepPointer, CompilerBundle *cb);
LLVMValueRef ac_compile_function_call(AST *ast, CompilerBundle *cb);

void die(int lineno, const char *fmt, ...)
{
    size_t len = strlen(fmt);
    char format[len + 9];
    sprintf(format, "Error: %s\n", fmt);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\t-> Line %d\n", lineno);
    va_end(args);

    exit(0);
}

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
            die(ALN, "Unknown value type.");
            return NULL;
    }
}

LLVMValueRef ac_compile_identifier(AST *ast, CompilerBundle *cb)
{
    ASTValue *a = (ASTValue *)ast;
    VarBundle *b = vs_get(cb->varScope, a->value.id);

    if(!b) // We are dealing with a local variable
        die(ALN, "Undeclared Identifier (%s)", a->value.id);

    if(b->type->type == ETFunction)
    {
        a->resultantType = b->type;
        return b->value;
    }

    a->resultantType = b->type;
    return LLVMBuildLoad(cb->builder, b->value, "loadtmp");
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

LLVMValueRef ac_compile_cast(AST *ast, CompilerBundle *cb)
{
    ASTCast *a = (ASTCast *)ast;
    ASTTypeDecl *ty = (ASTTypeDecl *)a->etype;

    LLVMValueRef val = ac_dispatch_expression(a->val, cb);

    EagleTypeType *to = ty->etype;
    EagleTypeType *from = a->val->resultantType;

    ast->resultantType = to;

    if(ett_is_numeric(to) && ett_is_numeric(from))
    {
        return ac_build_conversion(cb->builder, val, from, to);
    }

    if(to->type == ETPointer && from->type == ETPointer)
    {
        return LLVMBuildBitCast(cb->builder, val, ett_llvm_type(to), "casttmp");
    }

    if(to->type == ETPointer)
    {
        if(!ET_IS_INT(from->type))
            die(ALN, "Cannot cast non-integer type to pointer.");
        return LLVMBuildIntToPtr(cb->builder, val, ett_llvm_type(to), "casttmp");
    }

    if(from->type == ETPointer)
    {
        if(!ET_IS_INT(to->type))
            die(ALN, "Pointers may only be cast to other pointers or integers.");
        return LLVMBuildPtrToInt(cb->builder, val, ett_llvm_type(to), "casttmp");
    }

    die(ALN, "Unknown type conversion requested.");

    return NULL;
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
        if(IS_ANY_PTR(l->val->resultantType))
        {
            fprintf(stderr, "Error: Any pointers may not be dereferenced without cast.\n");
            exit(1);
        }
        totype = ((EaglePointerType *)l->val->resultantType)->to;
    }
    else if(a->left->type == AVARDECL)
    {
        pos = ac_dispatch_expression(a->left, cb);
        totype = a->left->resultantType;
    }
    else if(a->left->type == ABINARY && ((ASTBinary *)a->left)->op == '[')
    {
        pos = ac_compile_index(a->left, 1, cb);
        totype = a->left->resultantType;
    }
    else
    {
        die(ALN, "Left hand side may not be assigned to.");
        return NULL;
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

    if(!ett_are_same(fromtype, totype))
        r = ac_build_conversion(cb->builder, r, fromtype, totype);

    LLVMBuildStore(cb->builder, r, pos);
    return LLVMBuildLoad(cb->builder, pos, "loadtmp");
}

LLVMValueRef ac_compile_index(AST *ast, int keepPointer, CompilerBundle *cb)
{
    ASTBinary *a = (ASTBinary *)ast;
    AST *left = a->left;
    AST *right = a->right;

    LLVMValueRef l = ac_dispatch_expression(left, cb);
    LLVMValueRef r = ac_dispatch_expression(right, cb);

    EagleTypeType *lt = left->resultantType;
    EagleTypeType *rt = right->resultantType;

    if(lt->type != ETPointer)
        die(LN(left), "Only pointer types may be indexed.");
    if(ett_pointer_depth(lt) == 1 && ett_get_base_type(lt) == ETAny)
        die(LN(left), "Trying to dereference any-pointer.");
    if(!ett_is_numeric(rt))
        die(LN(right), "Arrays can only be indexed by a number.");

    if(ET_IS_REAL(rt->type))
        r = ac_build_conversion(cb->builder, r, rt, ett_base_type(ETInt64));

    ast->resultantType = ((EaglePointerType *)lt)->to;
    LLVMValueRef gep = LLVMBuildInBoundsGEP(cb->builder, l, &r, 1, "idx");

    if(keepPointer)
        return gep;

    return LLVMBuildLoad(cb->builder, gep, "dereftmp");
}

LLVMValueRef ac_compile_binary(AST *ast, CompilerBundle *cb)
{
    ASTBinary *a = (ASTBinary *)ast;
    if(a->op == '=')
        return ac_build_store(ast, cb);
    else if(a->op == '[')
        return ac_compile_index(ast, 0, cb);

    LLVMValueRef l = ac_dispatch_expression(a->left, cb);
    LLVMValueRef r = ac_dispatch_expression(a->right, cb);

    if(a->left->resultantType->type == ETPointer || a->right->resultantType->type == ETPointer)
    {
        EagleTypeType *lt = a->left->resultantType;
        EagleTypeType *rt = a->right->resultantType;
        if(a->op != '+')
            die(ALN, "Operation '%c' not valid for pointer types.", a->op);
        if(lt->type == ETPointer && !ET_IS_INT(rt->type))
            die(ALN, "Pointer arithmetic is only valid with integer and non-any pointer types.");
        if(rt->type == ETPointer && !ET_IS_INT(lt->type))
            die(ALN, "Pointer arithmetic is only valid with integer and non-any pointer types.");
        
        if(lt->type == ETPointer && ett_get_base_type(lt) == ETAny && ett_pointer_depth(lt) == 1)
            die(ALN, "Pointer arithmetic results in dereferencing any pointer.");
        if(rt->type == ETPointer && ett_get_base_type(rt) == ETAny && ett_pointer_depth(rt) == 1)
            die(ALN, "Pointer arithmetic results in dereferencing any pointer.");

        LLVMValueRef indexer = lt->type == ETPointer ? r : l;
        LLVMValueRef ptr = lt->type == ETPointer ? l : r;

        EaglePointerType *pt = lt->type == ETPointer ?
            (EaglePointerType *)lt : (EaglePointerType *)rt;

        ast->resultantType = (EagleTypeType *)pt;

        LLVMValueRef gep = LLVMBuildInBoundsGEP(cb->builder, ptr, &indexer, 1, "arith");
        return LLVMBuildBitCast(cb->builder, gep, ett_llvm_type((EagleTypeType *)pt), "cast");
    }

    EagleType promo = et_promotion(a->left->resultantType->type, a->right->resultantType->type);
    a->resultantType = ett_base_type(promo);

    if(a->left->resultantType->type != promo)
    {
        l = ac_build_conversion(cb->builder, l, a->left->resultantType, ett_base_type(promo));
    }
    else if(a->right->resultantType->type != promo)
    {
        r = ac_build_conversion(cb->builder, r, a->right->resultantType, ett_base_type(promo));
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
            ast->resultantType = ett_base_type(ETInt1);
            return ac_make_comp(l, r, cb->builder, promo, a->op);
        default:
            die(ALN, "Invalid binary operation (%c).", a->op);
            return NULL;
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
                die(LN(of), "Undeclared identifier (%s)", o->value.id);
            
            of->resultantType = b->type;

            return b->value;
        }
        case ABINARY:
        {
            ASTBinary *o = (ASTBinary *)of;
            if(o->op != '[')
                die(LN(of), "Address may not be taken of this operator.");

            LLVMValueRef val = ac_compile_index(of, 1, cb);
            return val;
        }
        default:
            die(LN(of), "Address may not be taken of this expression.");
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
                    case ETInt1:
                    case ETInt8:
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
                        die(ALN, "The requested type may not be printed.");
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
                    die(ALN, "Only pointers may be dereferenced.");
                if(IS_ANY_PTR(a->val->resultantType))
                    die(ALN, "Any pointers may not be dereferenced without cast.");

                LLVMValueRef r = LLVMBuildLoad(cb->builder, v, "dereftmp");
                EaglePointerType *pt = (EaglePointerType *)a->val->resultantType;
                a->resultantType = pt->to;
                return r;
            }
        case '!':
            // TODO: Broken
            return LLVMBuildNot(cb->builder, v, "nottmp");
        default:
            die(ALN, "Invalid unary operator (%c).", a->op);
            break;
    }

    return NULL;
}

LLVMValueRef ac_dispatch_expression(AST *ast, CompilerBundle *cb)
{
    LLVMValueRef val = NULL;
    switch(ast->type)
    {
        case AVALUE:
            val = ac_compile_value(ast, cb);
            break;
        case ABINARY:
            val = ac_compile_binary(ast, cb);
            break;
        case AUNARY:
            val = ac_compile_unary(ast, cb);
            break;
        case AVARDECL:
            val = ac_compile_var_decl(ast, cb);
            break;
        case AIDENT:
            val = ac_compile_identifier(ast, cb);
            break;
        case AFUNCCALL:
            val = ac_compile_function_call(ast, cb);
            break;
        case ACAST:
            val = ac_compile_cast(ast, cb);
            break;
        default:
            die(ALN, "Invalid expression type.");
            return NULL;
    }

    if(!ast->resultantType)
        die(ALN, "Internal Error. AST Resultant Type for expression not set.");

    return val;
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
        case ACAST:
            ac_compile_cast(ast, cb);
            break;
        default:
            die(ALN, "Invalid statement type.");
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
            die(ALN, "Invalid declaration type.");
            return;
    }
}

void ac_compile_if(AST *ast, CompilerBundle *cb, LLVMBasicBlockRef mergeBB)
{
    ASTIfBlock *a = (ASTIfBlock *)ast;
    LLVMValueRef val = ac_dispatch_expression(a->test, cb);

    LLVMValueRef cmp = NULL;
    if(a->test->resultantType->type == ETInt1)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntNE, val, LLVMConstInt(LLVMInt1Type(), 0, 0), "cmp");
    else if(a->test->resultantType->type == ETInt8)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntNE, val, LLVMConstInt(LLVMInt8Type(), 0, 0), "cmp");
    else if(a->test->resultantType->type == ETInt32)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntNE, val, LLVMConstInt(LLVMInt32Type(), 0, 0), "cmp");
    else if(a->test->resultantType->type == ETInt64)
        cmp = LLVMBuildICmp(cb->builder, LLVMIntNE, val, LLVMConstInt(LLVMInt64Type(), 0, 0), "cmp");
    else if(a->test->resultantType->type == ETDouble)
        cmp = LLVMBuildFCmp(cb->builder, LLVMRealONE, val, LLVMConstReal(LLVMDoubleType(), 0.0), "cmp");
    else
        die(ALN, "Cannot test against given type.");

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
            EagleTypeType *t = ((ASTUnary *)ast)->val->resultantType;
            EagleTypeType *o = cb->currentFunctionType->retType;

            if(!ett_are_same(t, o))
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
        EagleTypeType *rt = p->resultantType;
        if(!ett_are_same(rt, ett->params[i]))
            val = ac_build_conversion(cb->builder, val, rt, ett->params[i]);
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

    if(!ac_compile_block(a->body, entry, cb) && retType->etype->type != ETVoid)
        die(ALN, "Function must return a value.");

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

LLVMValueRef ac_build_conversion(LLVMBuilderRef builder, LLVMValueRef val, EagleTypeType *from, EagleTypeType *to)
{
    switch(from->type)
    {
        case ETPointer:
        {
            if(to->type != ETPointer)
                die(-1, "Non-pointer type may not be converted to pointer type.");

            if(ett_get_base_type(to) == ETAny && ett_pointer_depth(to) == 1)
                return LLVMBuildBitCast(builder, val, ett_llvm_type(to), "ptrtmp");
            if(ett_get_base_type(from) == ETAny && ett_pointer_depth(from) == 1)
                return LLVMBuildBitCast(builder, val, ett_llvm_type(to), "ptrtmp");

            if(ett_pointer_depth(to) != ett_pointer_depth(from))
                die(-1, "Implicit pointer conversion invalid. Cannot conver pointer of depth %d to depth %d.", ett_pointer_depth(to), ett_pointer_depth(from));
            if(ett_get_base_type(to) != ett_get_base_type(from))
                die(-1, "Implicit pointer conversion invalid; pointer types are incompatible.");

            return LLVMBuildBitCast(builder, val, ett_llvm_type(to), "ptrtmp");
        }
        case ETInt8:
        case ETInt32:
        case ETInt64:
            switch(to->type)
            {
                case ETInt8:
                case ETInt32:
                case ETInt64:
                    return LLVMBuildIntCast(builder, val, ett_llvm_type(to), "conv");
                case ETDouble:
                    return LLVMBuildSIToFP(builder, val, LLVMDoubleType(), "conv");
                default:
                    die(-1, "Invalid implicit conversion.");
                    break;
            }
        case ETDouble:
            switch(to->type)
            {
                case ETDouble:
                    return val;
                case ETInt8:
                case ETInt32:
                case ETInt64:
                    return LLVMBuildFPToSI(builder, val, ett_llvm_type(to), "conv");
                default:
                    die(-1, "Invalid implicit conversion from double.");
                    break;
            }
            break;
        default:
            die(-1, "Invalid implicit conversion.");
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
        case ETInt8:
        case ETInt32:
        case ETInt64:
            return LLVMBuildAdd(builder, left, right, "addtmp");
        default:
            die(-1, "The given types may not be summed.");
            return NULL;
    }
}

LLVMValueRef ac_make_sub(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type)
{
    switch(type)
    {
        case ETDouble:
            return LLVMBuildFSub(builder, left, right, "subtmp");
        case ETInt8:
        case ETInt32:
        case ETInt64:
            return LLVMBuildSub(builder, left, right, "subtmp");
        default:
            die(-1, "The given types may not be subtracted.");
            return NULL;
    }
}

LLVMValueRef ac_make_mul(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type)
{
    switch(type)
    {
        case ETDouble:
            return LLVMBuildFMul(builder, left, right, "multmp");
        case ETInt8:
        case ETInt32:
        case ETInt64:
            return LLVMBuildMul(builder, left, right, "multmp");
        default:
            die(-1, "The given types may not be multiplied.");
            return NULL;
    }
}

LLVMValueRef ac_make_div(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type)
{
    switch(type)
    {
        case ETDouble:
            return LLVMBuildFDiv(builder, left, right, "divtmp");
        case ETInt8:
        case ETInt32:
        case ETInt64:
            return LLVMBuildSDiv(builder, left, right, "divtmp");
        default:
            die(-1, "The given types may not be divided.");
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
        case ETInt8:
        case ETInt32:
        case ETInt64:
            return LLVMBuildICmp(builder, ip, left, right, "eqtmp");
        default:
            die(-1, "The given types may not be compared.");
            return NULL;
    }
}

