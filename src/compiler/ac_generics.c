/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ast_compiler.h"
#include "core/stringbuilder.h"

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
            /*
            EaglePointerType *ptt = NULL;
            if(param->type == ETPointer)
                param = ett_get_root_pointee_and_parent(param, (EagleComplexType **)&ptt);

            void *dup = malloc(ty_type_max_size());
            memcpy(dup, param, ty_type_max_size());

            if(ptt)
                ptt->to = dup;
            else
                ft->params[i] = dup;
                */
        }
    }

    EagleComplexType *rt = ft->retType;
    if(ett_qualifies_as_generic(rt))
    {
        ft->retType = ett_deep_copy(ft->retType);
        /*
        EaglePointerType *ptt = NULL;
        if(rt->type == ETPointer)
            rt = ett_get_root_pointee_and_parent(rt, (EagleComplexType **)&ptt);

        void *dup = malloc(ty_type_max_size());
        memcpy(dup, rt, ty_type_max_size());

        if(ptt)
            ptt->to = dup;
        else
            ft->retType = dup;
            */
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

    return 0;
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

static EagleComplexType *ac_resolve_types(EagleComplexType *generic, EagleComplexType *given, int lineno)
{
    if(generic->type != ETPointer)
        return given;

    if(given->type != ETPointer)
        die(lineno, "Generic function expected pointer but none was passed");

    int generic_depth = ett_pointer_depth(generic);
    int given_depth = ett_pointer_depth(given);
    if(generic_depth > given_depth)
        die(lineno, "Generic function requires pointer depth greater than that given");

    if(generic_depth == given_depth)
        return ett_get_root_pointee(given);

    int required_depth = given_depth - generic_depth;
    for(int i = 0; i < required_depth; i++)
    {
        given = ET_POINTEE(given);
    }

    return given;
}

LLVMValueRef ac_generic_get(char *func, EagleComplexType *arguments[], EagleComplexType **out_type, CompilerBundle *cb, int lineno)
{
    GenericBundle *gb = hst_get(&cb->genericFunctions, func, NULL, NULL);
    if(!gb)
        die(lineno, "Internal compiler error: could not find generic bundle");

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
            Arraylist generics = ett_get_generic_children(p);

            EagleGenericType *g = generics.items[0];
            arr_free(&generics);

            EagleComplexType *prev = hst_get(&scanned, g->ident, NULL, NULL);
            EagleComplexType *replacement_type = ac_resolve_types(p, given, lineno);

            new_args[i] = given;
            if(prev)
            {
                if(!ett_are_same(prev, replacement_type))
                    die(lineno, "Two different concrete types assigned to same generic type (%s)", g->ident);
                continue;
            }

            hst_put(&scanned, g->ident, replacement_type, NULL, NULL);

            char *type = ett_unique_type_name(replacement_type);
            sb_append(&sbd, type);
            free(type);
        }
    }

    EagleComplexType *retType = ft->retType;
    if(ett_qualifies_as_generic(retType))
    {
        if(retType->type == ETPointer)
        {
            retType = ett_deep_copy(retType);
            EagleComplexType *parent = NULL;
            EagleGenericType *gt = (EagleGenericType *)ett_get_root_pointee_and_parent(retType, &parent);
            ((EaglePointerType *)parent)->to = hst_get(&scanned, gt->ident, NULL, NULL);
        }
        else
        {
            retType = hst_get(&scanned, ((EagleGenericType *)retType)->ident, NULL, NULL);
        }
    }

    if(!retType)
        die(gb->definition->lineno, "Return type of generic function is unspecified");

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
    // ac_compile_function_ex(gb->definition, cb, concrete_func, (EagleFunctionType *)*out_type);
    hst_put(&gb->implementations, expanded_name, concrete_func, NULL, NULL);

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

