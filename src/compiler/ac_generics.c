/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ast_compiler.h"
#include "core/stringbuilder.h"

static void
ac_copy_and_find_types(EagleComplexType *reftype, EagleComplexType *intype, EagleComplexType **copied, Hashtable *scanned, int lineno);

typedef struct
{
    AST *definition;
    Hashtable implementations;
    EagleComplexType *template_type;
} GenericBundle;

typedef struct
{
    AST *definition;
    Hashtable scanned;
    LLVMValueRef concrete_func;
    EagleComplexType *concrete_type;
} GenericWork;

static GenericBundle *ac_gb_alloc(AST *ast, EagleComplexType *template_type)
{
    GenericBundle *bundle = malloc(sizeof(GenericBundle));
    bundle->definition = ast;
    bundle->implementations = hst_create();
    bundle->implementations.duplicate_keys = 1;

    bundle->template_type = template_type;

    // We need to replace all of the "tagged" template types with
    // concrete types as the original ones will be replaced later
    // on.
    EagleFunctionType *ft = (EagleFunctionType *)template_type;
    for(int i = 0; i < ft->pct; i++)
    {
        EagleComplexType *param = ft->params[i];
        if(ett_qualifies_as_generic(param))
        {
            ft->params[i] = ett_deep_copy(ft->params[i]);
        }
    }

    EagleComplexType *rt = ft->retType;
    if(ett_qualifies_as_generic(rt))
    {
        ft->retType = ett_deep_copy(ft->retType);
    }
    
    return bundle;
}

static GenericWork *ac_gw_alloc(AST *definition, Hashtable scanned, LLVMValueRef concrete_func, EagleComplexType *concrete_type)
{
    GenericWork *work = malloc(sizeof(GenericWork));
    work->definition = definition;
    work->scanned = scanned;
    work->concrete_func = concrete_func;
    work->concrete_type = concrete_type;

    return work;
}

int ac_decl_is_generic(AST *ast)
{
    if(ast->type != AFUNCDECL)
        return 0;

    ASTFuncDecl *a = (ASTFuncDecl *)ast;

    AST *param = a->params;
    while(param)
    {
        ASTVarDecl *v = (ASTVarDecl *)param;
        ASTTypeDecl *t = (ASTTypeDecl *)v->atype;
        EagleComplexType *type = t->etype;

        if(ett_qualifies_as_generic(type))
            return 1;

        param = param->next;
    }

    return ett_qualifies_as_generic(((ASTTypeDecl *)a->retType)->etype);
}

void ac_generic_check_ident(AST *ast, void *data)
{
    ASTValue *v = (ASTValue *)ast;
    void **items = data;

    CompilerBundle *cb = items[0];
    char *genname = items[1];
    char *ident = v->value.id;

    if(vs_is_in_global_scope(cb->varScope, ident))
    {
        VarBundle *vb = vs_get(cb->varScope, ident);
        if(vb->type->type != ETFunction)
            return;

        if(!ec_was_exported(cb->exports, ident, TFUNC))
            die(ALN, msgerr_generic_references_non_exported, genname, ident);
    }
}

void ac_generic_register(AST *ast, EagleComplexType *template_type, CompilerBundle *cb)
{
    ASTFuncDecl *a = (ASTFuncDecl *)ast;

    GenericBundle *gb = ac_gb_alloc(ast, template_type);
    hst_put(&cb->genericFunctions, a->ident, gb, NULL, NULL);
}

static void ac_replace_types_each(void *key, void *val, void *data)
{
    ty_set_generic_type(key, val);
}

static void ac_generic_compile_work(GenericWork *work, CompilerBundle *cb)
{
    hst_for_each(&work->scanned, &ac_replace_types_each, NULL);
    ac_compile_function_ex(work->definition, cb, work->concrete_func, (EagleFunctionType *)work->concrete_type);
}

static void ac_check_generics_each(void *key, void *val, void *data)
{
    char *ident = key;
    GenericBundle *gb = val;
    CompilerBundle *cb = data;

    if(ec_was_exported(cb->exports, ident, TFUNC))
    {
        AST *def = (AST *)gb->definition;
        ASTWalker *walker = aw_create();

        void *items[] = { cb, ident };
        aw_register(walker, AIDENT, ac_generic_check_ident, items);
        aw_walk(walker, def);
        aw_free(walker);
    }
}

void ac_check_generics(CompilerBundle *cb)
{
    hst_for_each(&cb->genericFunctions, ac_check_generics_each, cb);
}

void ac_compile_generics(CompilerBundle *cb)
{
    for(int i = 0; i < cb->genericWorkList.count; i++)
    {
        GenericWork *work = cb->genericWorkList.items[i];
        ac_generic_compile_work(work, cb);
        hst_free(&work->scanned);
        free(work);
    }
}

static void ac_handle_generic(EagleComplexType *reftype, EagleComplexType *intype, Hashtable *scanned, int lineno)
{
    if(!intype)
        return;

    EagleGenericType *g = (EagleGenericType *)reftype;
    EagleComplexType *prev = hst_get(scanned, g->ident, NULL, NULL);
    if(prev)
    {
        if(!ett_are_same(prev, intype))
            die(lineno, msgerr_generic_type_disagreement, g->ident);
    }

    hst_put(scanned, g->ident, intype, NULL, NULL);
}

static EagleComplexType *ac_handle_pointer(EagleComplexType *reftype, EagleComplexType *intype, Hashtable *scanned, int lineno)
{
    EaglePointerType *refp = (EaglePointerType *)reftype;
    EaglePointerType *inp  = (EaglePointerType *)intype;

    if(inp)
    {
        if(refp->counted != inp->counted)
            die(lineno, msgerr_generic_pointer_disagreement);
    }

    EagleComplexType *out = ett_copy(reftype);
    EaglePointerType *outp = (EaglePointerType *)out;

    ac_copy_and_find_types(refp->to, inp ? inp->to : NULL, &outp->to, scanned, lineno);
    return out;
}

static EagleComplexType *ac_handle_function(EagleComplexType *reftype, EagleComplexType *intype, Hashtable *scanned, int lineno)
{
    EagleFunctionType *reff = (EagleFunctionType *)reftype;
    EagleFunctionType *inf  = (EagleFunctionType *)intype;

    if(inf)
    {
        if(reff->pct != inf->pct)
            die(lineno, msgerr_generic_param_count_disagreement);
        if(reff->gen != inf->gen)
            die(lineno, msgerr_generic_function_type_disagreement);
        if(reff->variadic != inf->variadic)
            die(lineno, msgerr_generic_function_type_disagreement);
    }

    EagleComplexType *out = ett_copy(reftype);
    EagleFunctionType *outf = (EagleFunctionType *)out;

    for(int i = 0; i < outf->pct; i++)
        ac_copy_and_find_types(reff->params[i], inf ? inf->params[i] : NULL, &outf->params[i], scanned, lineno);
    ac_copy_and_find_types(reff->retType, inf ? inf->retType : NULL, &outf->retType, scanned, lineno);

    return out;
}

static void
ac_copy_and_find_types(EagleComplexType *reftype, EagleComplexType *intype, EagleComplexType **copied, Hashtable *scanned, int lineno)
{
    // If intype is not specified, we are *just* creating a copy and setting generics
    if(intype)
    {
        if(reftype->type != ETGeneric && reftype->type != intype->type)
            die(lineno, msgerr_generic_given_mismatch);
    }

    switch(reftype->type)
    {
        case ETGeneric:
            ac_handle_generic(reftype, intype, scanned, lineno);
            *copied = (intype ? intype : hst_get(scanned, ((EagleGenericType *)reftype)->ident, NULL, NULL));
            if(!*copied)
                die(lineno, msgerr_generic_inference_impossible, ((EagleGenericType *)reftype)->ident);
            break;
        case ETPointer:
            *copied = ac_handle_pointer(reftype, intype, scanned, lineno);
            break;
        case ETFunction:
            *copied = ac_handle_function(reftype, intype, scanned, lineno);
            break;
        default:
            break;
    }
}

LLVMValueRef ac_generic_get(char *func, EagleComplexType *arguments[], EagleComplexType **out_type, CompilerBundle *cb, int lineno)
{
    GenericBundle *gb = hst_get(&cb->genericFunctions, func, NULL, NULL);
    if(!gb)
        die(lineno, msgerr_internal_no_generic_bundle);

    Strbuilder sbd;
    sb_init(&sbd);
    sb_append(&sbd, func);

    EagleFunctionType *ft = (EagleFunctionType *)gb->template_type;

    Hashtable scanned = hst_create();

    EagleComplexType *new_args[ft->pct];
    memcpy(new_args, ft->params, sizeof(EagleComplexType *) * ft->pct);

    for(int i = 0; i < ft->pct; i++)
    {
        EagleComplexType *p = ft->params[i];
        EagleComplexType *given = arguments[i];
        if(ett_qualifies_as_generic(p))
        {
            EagleComplexType *replacement_type = NULL;
            ac_copy_and_find_types(p, given, &replacement_type, &scanned, lineno);

            new_args[i] = given;

            char *type = ett_unique_type_name(replacement_type);
            sb_append(&sbd, type);
            free(type);
        }
    }

    EagleComplexType *retType = ft->retType;
    if(ett_qualifies_as_generic(retType))
        ac_copy_and_find_types(ft->retType, NULL, &retType, &scanned, lineno);

    if(!retType)
        die(gb->definition->lineno, msgerr_generic_unspecified_return);

    *out_type = ett_function_type(retType, new_args, ft->pct);

    char *expanded_name = sbd.buffer;
    LLVMValueRef concrete_func = hst_get(&gb->implementations, expanded_name, NULL, NULL);
    if(concrete_func)
    {
        free(expanded_name);
        return concrete_func;
    }

    concrete_func = LLVMAddFunction(cb->module, expanded_name, ett_llvm_type(*out_type));
    arr_append(&cb->genericWorkList, ac_gw_alloc(gb->definition, scanned, concrete_func, *out_type));
    hst_put(&gb->implementations, expanded_name, concrete_func, NULL, NULL);
    LLVMSetLinkage(concrete_func, LLVMLinkOnceODRLinkage);

    free(expanded_name);
    return concrete_func;
}

static void ac_generics_cleanup_each(void *key, void *val, void *data)
{
    GenericBundle *bundle = val;
    hst_free(&bundle->implementations);
    free(bundle);
}

void ac_generics_cleanup(CompilerBundle *cb)
{
    hst_for_each(&cb->genericFunctions, &ac_generics_cleanup_each, NULL);
}

