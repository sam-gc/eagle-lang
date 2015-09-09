#include "ast_compiler.h"

LLVMValueRef ac_compile_value(AST *ast, CompilerBundle *cb)
{
    ASTValue *a = (ASTValue *)ast;
    switch(a->etype)
    {
        case ETInt1:
            a->resultantType = ett_base_type(ETInt1);
            return LLVMConstInt(LLVMInt1Type(), a->value.i, 1);
        case ETInt32:
            a->resultantType = ett_base_type(ETInt32);
            return LLVMConstInt(LLVMInt32Type(), a->value.i, 1);
        case ETDouble:
            a->resultantType = ett_base_type(ETDouble);
            return LLVMConstReal(LLVMDoubleType(), a->value.d);
        case ETNil:
            a->resultantType = ett_pointer_type(ett_base_type(ETAny));
            return LLVMConstPointerNull(ett_llvm_type(a->resultantType));
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

    if(ET_IS_CLOSED(b->type))
    {
        a->resultantType = ((EaglePointerType *)b->type)->to;
        LLVMValueRef pos = LLVMBuildStructGEP(cb->builder, LLVMBuildLoad(cb->builder, b->value, ""), 5, "");

        if(a->resultantType->type == ETArray || a->resultantType->type == ETStruct)
            return pos;

        return LLVMBuildLoad(cb->builder, pos, "loadtmp");
    }

    a->resultantType = b->type;

    if(b->type->type == ETArray && !ET_IS_GEN_ARR(b->type))
        return b->value;

    if(b->type->type == ETStruct)
        return b->value;
    /*
    if((b->type->type == ETPointer && ((EaglePointerType *)b->type)->to->type == ETStruct) && (!ET_IS_COUNTED(b->type) && !ET_IS_WEAK(b->type)))
        return b->value;
        */

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

    LLVMPositionBuilderAtEnd(cb->builder, curblock);

    vs_put(cb->varScope, a->ident, pos, type->etype);

    if(ET_IS_COUNTED(type->etype))
    {
        LLVMBuildStore(cb->builder, LLVMConstPointerNull(ett_llvm_type(type->etype)), pos);

        vs_add_callback(cb->varScope, a->ident, ac_scope_leave_callback, cb);
    }
    else if(ET_IS_WEAK(type->etype))
    {
        LLVMBuildStore(cb->builder, LLVMConstPointerNull(ett_llvm_type(type->etype)), pos);
        vs_add_callback(cb->varScope, a->ident, ac_scope_leave_weak_callback, cb);
    }
    else if(type->etype->type == ETStruct && ty_needs_destructor(type->etype))
    {
        ac_call_constructor(cb, pos, type->etype);
        vs_add_callback(cb->varScope, a->ident, ac_scope_leave_struct_callback, cb);
    }

    if(type->etype->type == ETArray && ett_array_has_counted(type->etype))
    {
        ac_nil_fill_array(cb, pos, ett_array_count(type->etype));
        vs_add_callback(cb->varScope, a->ident, ac_scope_leave_array_callback, cb);
    }

    ast->resultantType = type->etype;

    return pos;
}

LLVMValueRef ac_compile_struct_member(AST *ast, CompilerBundle *cb, int keepPointer)
{
    ASTStructMemberGet *a = (ASTStructMemberGet *)ast;
    LLVMValueRef left = ac_dispatch_expression(a->left, cb);

    EagleTypeType *ty = a->left->resultantType;

    if(ty->type != ETStruct)
        die(ALN, "Attempting to access member of non-struct type (%s).", a->ident);
    if(ty->type == ETPointer && ((EaglePointerType *)ty)->to->type != ETStruct)
        die(ALN, "Attempting to access member of non-struct pointer type (%s).", a->ident);

    int index;
    EagleTypeType *type;
    ty_struct_member_index(ty, a->ident, &index, &type);

    if(index < -1)
        die(ALN, "Internal compiler error. Struct not loaded but found.");
    if(index < 0)
        die(ALN, "Struct \"%s\" has no member \"%s\".", ((EagleStructType *)ty)->name, a->ident);

    ast->resultantType = type;

    LLVMValueRef gep = LLVMBuildStructGEP(cb->builder, left, index, a->ident);
    if(keepPointer || type->type == ETStruct)
        return gep;
    return LLVMBuildLoad(cb->builder, gep, "");
}

LLVMValueRef ac_compile_malloc_counted_raw(LLVMTypeRef rt, LLVMTypeRef *out, CompilerBundle *cb)
{
    LLVMTypeRef ptmp[2];
    ptmp[0] = LLVMPointerType(LLVMInt8Type(), 0);
    ptmp[1] = LLVMInt1Type();
    
    LLVMTypeRef tys[6];
    tys[0] = LLVMInt64Type();
    tys[1] = LLVMInt16Type();
    tys[2] = LLVMInt16Type();
    tys[3] = LLVMPointerType(LLVMInt8Type(), 0);
    tys[4] = LLVMPointerType(LLVMFunctionType(LLVMVoidType(), ptmp, 2, 0), 0);
    tys[5] = rt;
    LLVMTypeRef tt = LLVMStructType(tys, 6, 0);
    tt = ty_get_counted(tt);
    
    LLVMValueRef mal = LLVMBuildMalloc(cb->builder, tt, "new");

    *out = tt;

    ac_prepare_pointer(cb, mal, NULL);

    return mal;
}

LLVMValueRef ac_compile_malloc_counted(EagleTypeType *type, EagleTypeType **res, LLVMValueRef ib, CompilerBundle *cb)
{
    LLVMTypeRef ptmp[2];
    ptmp[0] = LLVMPointerType(LLVMInt8Type(), 0);
    ptmp[1] = LLVMInt1Type();
    
    LLVMTypeRef tys[6];
    tys[0] = LLVMInt64Type();
    tys[1] = LLVMInt16Type();
    tys[2] = LLVMInt16Type();
    tys[3] = LLVMPointerType(LLVMInt8Type(), 0);
    tys[4] = LLVMPointerType(LLVMFunctionType(LLVMVoidType(), ptmp, 2, 0), 0);
    tys[5] = ett_llvm_type(type);
    LLVMTypeRef tt = LLVMStructType(tys, 6, 0);

    tt = ty_get_counted(tt);

    LLVMValueRef mal;
    if(ib)
        mal = EGLBuildMalloc(cb->builder, tt, ib, "new");
    else
        mal = LLVMBuildMalloc(cb->builder, tt, "new");
   
    EaglePointerType *pt = (EaglePointerType *)ett_pointer_type(type);
    pt->counted = 1;
    EagleTypeType *resultantType = (EagleTypeType *)pt;
    if(res)
        *res = resultantType;

    ac_prepare_pointer(cb, mal, resultantType);
    if(type->type == ETStruct && ty_needs_destructor(type))
    {
        LLVMValueRef pos = LLVMBuildStructGEP(cb->builder, mal, 5, "");
        ac_call_constructor(cb, pos, type);
        pos = LLVMBuildStructGEP(cb->builder, mal, 4, "");
        EagleStructType *st = (EagleStructType *)type;
        LLVMBuildStore(cb->builder, ac_gen_struct_destructor_func(st->name, cb), pos);
    }
    
    // We need to specially handle counted counted types
    if(ET_IS_COUNTED(type))
    {
        LLVMValueRef pos = LLVMBuildStructGEP(cb->builder, mal, 4, "");
        LLVMBuildStore(cb->builder, LLVMGetNamedFunction(cb->module, "__egl_counted_destructor"), pos);

        pos = LLVMBuildStructGEP(cb->builder, mal, 5, "");
        LLVMBuildStore(cb->builder, LLVMConstPointerNull(ett_llvm_type(type)), pos);
    }
    /*
    LLVMValueRef pos = LLVMBuildStructGEP(cb->builder, mal, 0, "ctp");
    LLVMBuildStore(cb->builder, LLVMConstInt(LLVMInt64Type(), 0, 0), pos);
    */


    return mal;
}

LLVMValueRef ac_compile_new_decl(AST *ast, CompilerBundle *cb)
{
    ASTUnary *a = (ASTUnary *)ast;
    ASTTypeDecl *type = (ASTTypeDecl *)a->val;
    
    LLVMValueRef val = ac_compile_malloc_counted(type->etype, &ast->resultantType, NULL, cb);
    hst_put(&cb->transients, ast, val, ahhd, ahed);

    return val;
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

LLVMValueRef ac_compile_index(AST *ast, int keepPointer, CompilerBundle *cb)
{
    ASTBinary *a = (ASTBinary *)ast;
    AST *left = a->left;
    AST *right = a->right;

    LLVMValueRef l = ac_dispatch_expression(left, cb);
    LLVMValueRef r = ac_dispatch_expression(right, cb);

    EagleTypeType *lt = left->resultantType;
    EagleTypeType *rt = right->resultantType;

    if(lt->type != ETPointer && lt->type != ETArray)
        die(LN(left), "Only pointer types may be indexed.");
    if(lt->type == ETPointer && ett_pointer_depth(lt) == 1 && ett_get_base_type(lt) == ETAny)
        die(LN(left), "Trying to dereference any-pointer.");
    if(!ett_is_numeric(rt))
        die(LN(right), "Arrays can only be indexed by a number.");

    if(ET_IS_REAL(rt->type))
        r = ac_build_conversion(cb->builder, r, rt, ett_base_type(ETInt64));

    if(lt->type == ETPointer)
        ast->resultantType = ((EaglePointerType *)lt)->to;
    else
        ast->resultantType = ((EagleArrayType *)lt)->of;

    LLVMValueRef gep;
    if(lt->type == ETArray && ((EagleArrayType *)lt)->ct >= 0)
    {
        LLVMValueRef zero = LLVMConstInt(LLVMInt32Type(), 0, 0);
        LLVMValueRef pts[] = {zero, r};
        gep = LLVMBuildInBoundsGEP(cb->builder, l, pts, 2, "idx");
    }
    else
    {
        gep = LLVMBuildInBoundsGEP(cb->builder, l, &r, 1, "idx");
    }

    if(keepPointer || (lt->type == ETArray && ((EagleArrayType *)lt)->of->type == ETArray))
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
        if(a->op != '+' && a->op != '-')
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

        if(a->op == '-')
            indexer = LLVMBuildNeg(cb->builder, indexer, "neg");

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
        case ASTRUCTMEMBER:
        {
            return ac_compile_struct_member(of, cb, 1);
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

    if(a->op == 't')
    {
        //ASTValue *v = (ASTValue *)a->val;
        //ac_replace_with_counted(cb, v->value.id);
        return NULL;
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
                        fmt = LLVMBuildGlobalStringPtr(cb->builder, "(Bool) %d\n", "prfB");
                        break;
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

                ac_unwrap_pointer(cb, &v, a->val->resultantType, 0);

                EaglePointerType *pt = (EaglePointerType *)a->val->resultantType;
                a->resultantType = pt->to;

                LLVMValueRef r = v;
                if(a->resultantType->type != ETStruct)
                    r = LLVMBuildLoad(cb->builder, v, "dereftmp");
                return r;
            }
        case 'c':
            {
                if(a->val->resultantType->type != ETArray)
                    die(ALN, "countof operator only valid for arrays.");

                LLVMValueRef r = LLVMBuildStructGEP(cb->builder, v, 0, "ctp");
                a->resultantType = ett_base_type(ETInt64);
                return LLVMBuildLoad(cb->builder, r, "dereftmp");
            }
        case 'u':
            {
                if(!ET_IS_COUNTED(a->val->resultantType) && !ET_IS_WEAK(a->val->resultantType))
                    die(ALN, "Only pointers in the counted regime may be unwrapped.");

                a->resultantType = ett_pointer_type(((EaglePointerType *)a->val->resultantType)->to);
                return LLVMBuildStructGEP(cb->builder, v, 5, "unwrap");
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

LLVMValueRef ac_compile_function_call(AST *ast, CompilerBundle *cb)
{
    ASTFuncCall *a = (ASTFuncCall *)ast;

    LLVMValueRef func = ac_dispatch_expression(a->callee, cb);

    EagleTypeType *orig = a->callee->resultantType;

    EagleFunctionType *ett;
    if(orig->type == ETFunction)
        ett = (EagleFunctionType *)orig;
    else
        ett = (EagleFunctionType *)((EaglePointerType *)orig)->to;

    a->resultantType = ett->retType;

    AST *p;
    int i;
    for(p = a->params, i = 0; p; p = p->next, i++);
    int ct = i;

    LLVMValueRef args[ct + 1];
    for(p = a->params, i = 0; p; p = p->next, i++)
    {
        LLVMValueRef val = ac_dispatch_expression(p, cb);
        EagleTypeType *rt = p->resultantType;
        if(!ett_are_same(rt, ett->params[i]))
            val = ac_build_conversion(cb->builder, val, rt, ett->params[i]);

        hst_remove_key(&cb->transients, p, ahhd, ahed);
        // hst_remove_key(&cb->loadedTransients, p, ahhd, ahed);
        args[i + 1] = val;
    }

    LLVMValueRef out;
    if(ET_IS_CLOSURE(ett))
    {
        // func = LLVMBuildLoad(cb->builder, func, "");
        if(!ET_IS_RECURSE(ett))
            ac_unwrap_pointer(cb, &func, NULL, 0);


        // if(ET_HAS_CLOASED(ett))
        args[0] = LLVMBuildLoad(cb->builder, LLVMBuildStructGEP(cb->builder, func, 1, ""), "");
        // else
        //     args[0] = LLVMConstPointerNull(LLVMPointerType(LLVMInt8Type(), 0));


        func = LLVMBuildStructGEP(cb->builder, func, 0, "");
        func = LLVMBuildLoad(cb->builder, func, "");
        func = LLVMBuildBitCast(cb->builder, func, LLVMPointerType(ett_closure_type((EagleTypeType *)ett), 0), "");
        
        out = LLVMBuildCall(cb->builder, func, args, ct + 1, "");
    }
    else
        out = LLVMBuildCall(cb->builder, func, args + 1, ct, ett->retType->type == ETVoid ? "" : "callout");

    if(ET_IS_COUNTED(ett->retType) || (ett->retType->type == ETStruct && ty_needs_destructor(ett->retType)))
    {
        hst_put(&cb->loadedTransients, ast, out, ahhd, ahed);
    }

    return out;
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

        if(ET_IS_CLOSED(b->type))
        {
            totype = ((EaglePointerType *)b->type)->to;
            pos = LLVMBuildLoad(cb->builder, b->value, "");
            pos = LLVMBuildStructGEP(cb->builder, pos, 5, "");
        }
        else
        {
            totype = b->type;
            pos = b->value;
        }
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
        ac_unwrap_pointer(cb, &pos, l->val->resultantType, 0);
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
    else if(a->left->type == ASTRUCTMEMBER)
    {
        pos = ac_compile_struct_member(a->left, cb, 1);
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

    int transient = 0;
    LLVMValueRef ptrPos = NULL;
    if(ET_IS_COUNTED(a->resultantType))
    {
        ptrPos = pos;
        ac_decr_pointer(cb, &pos, totype);

        /*if(hst_remove_key(&cb->transients, a->right, ahhd, ahed))
            transient = 1;*/
        hst_remove_key(&cb->transients, a->right, ahhd, ahed);
        if(hst_remove_key(&cb->loadedTransients, a->right, ahhd, ahed))
            transient = 1;
        //ac_unwrap_pointer(cb, &pos, totype, 1);
    }
    else if(ET_IS_WEAK(a->resultantType))
    {
        ac_remove_weak_pointer(cb, pos, totype);
        ac_add_weak_pointer(cb, r, pos, totype);
    }
    else if(a->resultantType->type == ETStruct && ty_needs_destructor(a->resultantType))
    {
        ac_call_destructor(cb, pos, a->resultantType);
        r = LLVMBuildLoad(cb->builder, r, "");
        if(hst_remove_key(&cb->loadedTransients, a->right, ahhd, ahed))
            transient = 1;
        //ac_call_constr(cb, r, a->resultantType);
    }

    LLVMBuildStore(cb->builder, r, pos);
    
    if(ET_IS_COUNTED(a->resultantType) && !transient)
        ac_incr_pointer(cb, &ptrPos, totype);
    else if(a->resultantType->type == ETStruct && ty_needs_destructor(a->resultantType) && !transient)
        ac_call_copy_constructor(cb, pos, a->resultantType);

    return LLVMBuildLoad(cb->builder, pos, "loadtmp");
}
