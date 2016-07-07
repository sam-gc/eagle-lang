/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "ast.h"
#include "ast_compiler.h"
#include "core/arraylist.h"
#include "core/mempool.h"

static Mempool ast_mempool;
static Mempool ast_lst_mempool;
static Mempool ast_hst_mempool;
static int init_pool = 0;

void ast_free_nodes()
{
    pool_drain(&ast_hst_mempool);
    pool_drain(&ast_lst_mempool);
    pool_drain(&ast_mempool);
    init_pool = 0;
}

void ast_free_lists(void *arr)
{
    arr_free(arr);
}

void ast_free_tables(void *hst)
{
    hst_free(hst);
}

extern int yylineno;
extern char *yytext;
int yyerror(const char *text)
{
    const char *format = strlen(yytext) == 0 ? "%s%s" : "%s (%s)";
    die(yylineno, format, text, yytext);
    return -1;
}

void *ast_malloc(size_t size)
{
    AST *ast = malloc(size);
    ast->next = NULL;
    ast->lineno = yylineno;

    if(!init_pool)
    {
        ast_mempool = pool_create();
        ast_lst_mempool = pool_create();
        ast_lst_mempool.free_func = ast_free_lists;
        ast_hst_mempool = pool_create();
        ast_hst_mempool.free_func = ast_free_tables;
        init_pool = 1;
    }

    pool_add(&ast_mempool, ast);

    return ast;
}

AST *ast_make()
{
    AST *ast = ast_malloc(sizeof(AST));
    ast->type = ABASE;

    return ast;
}

AST *ast_make_skip()
{
    AST *ast = ast_malloc(sizeof(AST));
    ast->type = ASKIP;

    return ast;
}

void ast_append(AST *n, AST *o)
{
    n->next = o;
}

AST *ast_make_binary(AST *left, AST *right, char op)
{
    ASTBinary *ast = ast_malloc(sizeof(ASTBinary));
    ast->type = ABINARY;

    ast->op = op;
    ast->left = left;
    ast->right = right;

    return (AST *)ast;
}

AST *ast_make_unary(AST *val, char op)
{
    ASTUnary *ast = ast_malloc(sizeof(ASTUnary));
    ast->type = AUNARY;

    ast->op = op;
    ast->val = val;

    return (AST *)ast;
}

AST *ast_make_allocater(char op, AST *val, AST *init)
{
    ASTTypeDecl *at = (ASTTypeDecl *)val;
    if(at->etype->type == ETClass)
    {
        if(!init)
            die(yylineno, msgerr_class_decl_no_parens);
        if((uintptr_t)init == 1)
            init = NULL; // We just use 1 to indicate that there is indeed parentheses, for consistency.
    }
    AST *ast = ast_make_binary(val, init, op);
    ast->type = AALLOC;
    return ast;
}

AST *ast_make_bool(int i)
{
    ASTValue *ast = ast_malloc(sizeof(ASTValue));
    ast->type = AVALUE;
    ast->etype = ETInt1;
    ast->value.i = i;

    return (AST *)ast;
}

AST *ast_make_nil()
{
    ASTValue *ast = ast_malloc(sizeof(ASTValue));
    ast->type = AVALUE;
    ast->etype = ETNil;

    return (AST *)ast;
}

AST *ast_make_int32(char *text)
{
    ASTValue *ast = ast_malloc(sizeof(ASTValue));
    ast->type = AVALUE;
    // ast->etype = ETInt32;

    int us, bits;
    ast->value.i = utl_process_int(text, &us, &bits);

    static EagleBasicType ust[] = {ETUInt8, ETUInt16, ETUInt32, ETUInt64};
    static EagleBasicType st[]  = {ETInt8,  ETInt16,  ETInt32,  ETInt64};

    EagleBasicType *src = us ? ust : st;
    switch(bits)
    {
        case 8:
            ast->etype = src[0];
            break;
        case 16:
            ast->etype = src[1];
            break;
        case 32:
            ast->etype = src[2];
            break;
        case 64:
            ast->etype = src[3];
            break;
    }

    return (AST *)ast;
}

AST *ast_make_byte(char *text)
{
    ASTValue *ast = ast_malloc(sizeof(ASTValue));
    ast->type = AVALUE;
    ast->etype = ETInt8;
    ast->value.i = text[1];

    return (AST *)ast;
}

AST *ast_make_double(char *text)
{
    ASTValue *ast = ast_malloc(sizeof(ASTValue));
    ast->type = AVALUE;
    ast->etype = ETDouble;
    ast->value.d = atof(text);

    return (AST *)ast;
}

AST *ast_make_cstr(char *text)
{
    ASTValue *ast = ast_malloc(sizeof(ASTValue));
    ast->type = AVALUE;
    ast->etype = ETCString;

    text += 1;
    text[strlen(text) - 1] = '\0';
    ast->value.id = text;
    // ast->value.id = strdup(text);

    return (AST *)ast;
}

AST *ast_make_identifier(char *ident)
{
    ASTValue *ast = ast_malloc(sizeof(ASTValue));
    ast->type = AIDENT;
    ast->etype = ETNone;
    ast->value.id = ident;

    return (AST *)ast;
}

AST *ast_set_vararg(AST *ast)
{
    ASTFuncDecl *a = (ASTFuncDecl *)ast;
    a->vararg = 1;

    return ast;
}

AST *ast_make_class_special_decl(char *ident, AST *body, AST *params)
{
    if(strcmp(ident, "init") && strcmp(ident, "destruct"))
        die(yylineno, msgerr_unexpected_identifier, ident);

    ASTFuncDecl *ast = ast_malloc(sizeof(ASTFuncDecl));
    ast->type = AFUNCDECL;
    ast->retType = ast_make_type((char *)"void");
    ast->body = body;
    ast->ident = ident;
    ast->params = params;
    ast->vararg = 0;

    return (AST *)ast;
}

AST *ast_make_func_decl(AST *type, char *ident, AST *body, AST *params)
{
    ASTFuncDecl *ast = ast_malloc(sizeof(ASTFuncDecl));
    ast->type = AFUNCDECL;
    ast->retType = type;
    ast->body = body;
    ast->ident = ident ? ident : (char *)"close";
    ast->params = params;
    ast->vararg = 0;
    ast->linkage = VLLocal;

    return (AST *)ast;
}

AST *ast_make_gen_decl(AST *type, char *ident, AST *body, AST *params)
{
    ASTFuncDecl *ast = ast_malloc(sizeof(ASTFuncDecl));
    ast->type = AGENDECL;
    ast->retType = type;
    ast->body = body;
    ast->ident = ident ? ident : (char *)"close";
    ast->params = params;

    return (AST *)ast;
}

AST *ast_make_func_call(AST *callee, AST *params)
{
    ASTFuncCall *ast = ast_malloc(sizeof(ASTFuncCall));
    ast->type = AFUNCCALL;

    ast->callee = callee;
    ast->params = params;

    return (AST *)ast;
}

AST *ast_make_var_decl(AST *atype, char *ident)
{
    return ast_make_arr_decl(atype, ident, NULL);
}

AST *ast_make_auto_decl(char *ident)
{
    ASTVarDecl *ast = ast_malloc(sizeof(ASTVarDecl));
    ast->type = AVARDECL;
    ast->atype = ast_make_type((char *)"auto");
    ast->ident = ident;
    ast->arrct = NULL;
    ast->linkage = VLNone;
    ast->staticInit = NULL;

    return (AST *)ast;
}

AST *ast_make_struct_decl()
{
    ASTStructDecl *ast = ast_malloc(sizeof(ASTStructDecl));
    ast->type = ASTRUCTDECL;

    ast->name = NULL;
    ast->names = arr_create(10);
    ast->types = arr_create(10);
    ast->ext = 0;

    pool_add(&ast_lst_mempool, &ast->names);
    pool_add(&ast_lst_mempool, &ast->types);

    return (AST *)ast;
}

AST *ast_struct_add(AST *ast, AST *var)
{
    ASTStructDecl *a = (ASTStructDecl *)ast;
    ASTVarDecl *v = (ASTVarDecl *)var;
    ASTTypeDecl *ty = (ASTTypeDecl *)v->atype;

    arr_append(&a->types, ty->etype);
    arr_append(&a->names, v->ident);

    return ast;
}

AST *ast_struct_name(AST *ast, char *name)
{
    ASTStructDecl *a = (ASTStructDecl *)ast;
    a->name = name;
    return ast;
}

void ast_struct_set_extern(AST *ast)
{
    ASTStructDecl *a = (ASTStructDecl *)ast;
    a->ext = 1;
}

AST *ast_make_struct_lit_dict()
{
    Hashtable *hst = malloc(sizeof(*hst));
    *hst = hst_create();

    // Pretend this is a tree node even though it is not
    return (AST *)hst;
}

void ast_struct_lit_add(void *dict, char *ident, AST *expr)
{
    hst_put(dict, ident, expr, NULL, NULL);
}

AST *ast_make_struct_lit(char *name, AST *dict)
{
    ASTStructLit *ast = ast_malloc(sizeof(ASTStructLit));
    ast->type = ASTRUCTLIT;

    Hashtable *hst = (Hashtable *)dict;
    ast->exprs = *hst;
    ast->name = name;

    return (AST *)ast;
}

AST *ast_make_interface_decl()
{
    AST *ast = ast_make_class_decl();
    ast->type = AIFCDECL;

    return ast;
}

AST *ast_make_class_decl()
{
    ASTClassDecl *ast = ast_malloc(sizeof(ASTClassDecl));
    ast->type = ACLASSDECL;
    ast->linkage = VLLocal;

    ast->name = NULL;
    ast->names = arr_create(10);
    ast->types = arr_create(10);

    ast->method_types = hst_create();
    ast->static_method_types = hst_create();
    ast->initdecl = NULL;
    ast->interfaces = arr_create(5);
    ast->destructdecl = NULL;

    pool_add(&ast_lst_mempool, &ast->names);
    pool_add(&ast_lst_mempool, &ast->types);
    pool_add(&ast_lst_mempool, &ast->interfaces);
    pool_add(&ast_hst_mempool, &ast->method_types);
    pool_add(&ast_hst_mempool, &ast->static_method_types);

    ast->ext = 0;

    return (AST *)ast;
}

void ast_class_set_extern(AST *ast)
{
    ASTClassDecl *cls = (ASTClassDecl *)ast;
    cls->ext = 1;
}

void ast_class_add_interface(AST *ast, AST *interfaces)
{
    ASTClassDecl *cls = (ASTClassDecl *)ast;

    ASTTypeDecl *i = (ASTTypeDecl *)interfaces;
    for(; i; i = (ASTTypeDecl *)i->next)
    {
        if(i->etype->type != ETInterface)
            die(ALN, msgerr_non_interface_type_in_impl_list, cls->name);
        EagleInterfaceType *ei = (EagleInterfaceType *)i->etype;

        if(ei->names.count != 1)
            die(ALN, msgerr_bad_composite_type_decl, cls->name);

        arr_append(&cls->interfaces, ei->names.items[0]);
    }
}

AST *ast_class_set_init(AST *ast, AST *init)
{
    ASTClassDecl *cls = (ASTClassDecl *)ast;
    ASTFuncDecl *f = (ASTFuncDecl *)init;

    AST *impl = ast_make_var_decl(ast_make_pointer(ast_make_type((char *)"any")), (char *)"self");
    impl->next = f->params;
    f->params = impl;

    Arraylist list = arr_create(10);
    ASTVarDecl *vd = (ASTVarDecl *)f->params;
    for(; vd; vd = (ASTVarDecl *)vd->next)
    {
        ASTTypeDecl *t = (ASTTypeDecl *)vd->atype;
        arr_append(&list, t->etype);
    }

    EagleComplexType *ttype = ett_function_type(((ASTTypeDecl *)f->retType)->etype, (EagleComplexType **)list.items, list.count);
    ((EagleFunctionType *)ttype)->closure = 0;

    arr_free(&list);
    if(strcmp(f->ident, "init") == 0)
    {
        cls->inittype = ttype;
        cls->initdecl = init;
    }
    else
    {
        if(((EagleFunctionType *)ttype)->pct > 1)
            die(yylineno, msgerr_destructor_has_params);
        cls->destructtype = ttype;
        cls->destructdecl = init;
    }

    return ast;
}

AST *ast_class_var_add(AST *ast, AST *var)
{
    ASTClassDecl *a = (ASTClassDecl *)ast;
    ASTVarDecl *v = (ASTVarDecl *)var;
    ASTTypeDecl *ty = (ASTTypeDecl *)v->atype;

    arr_append(&a->types, ty->etype);
    arr_append(&a->names, v->ident);

    return ast;
}

AST *ast_class_method_add(AST *ast, AST *func)
{
    ASTClassDecl *a = (ASTClassDecl *)ast;
    ASTFuncDecl *f = (ASTFuncDecl *)func;

    AST *impl = ast_make_var_decl(ast_make_pointer(ast_make_type((char *)"any")), (char *)"self");
    impl->next = f->params;
    f->params = impl;

    Arraylist list = arr_create(10);
    ASTVarDecl *vd = (ASTVarDecl *)f->params;
    for(; vd; vd = (ASTVarDecl *)vd->next)
    {
        ASTTypeDecl *t = (ASTTypeDecl *)vd->atype;
        arr_append(&list, t->etype);
    }

    EagleComplexType *ttype = ett_function_type(((ASTTypeDecl *)f->retType)->etype, (EagleComplexType **)list.items, list.count);
    arr_free(&list);

    hst_put(&a->method_types, f, ttype, ahhd, ahed);

    return ast;
}

AST *ast_class_static_method_add(AST *ast, AST *func)
{
    ASTClassDecl *a = (ASTClassDecl *)ast;
    ASTFuncDecl *f = (ASTFuncDecl *)func;

    Arraylist list = arr_create(10);
    ASTVarDecl *vd = (ASTVarDecl *)f->params;
    for(; vd; vd = (ASTVarDecl *)vd->next)
    {
        ASTTypeDecl *t = (ASTTypeDecl *)vd->atype;
        arr_append(&list, t->etype);
    }

    EagleComplexType *ttype = ett_function_type(((ASTTypeDecl *)f->retType)->etype, (EagleComplexType **)list.items, list.count);
    arr_free(&list);

    hst_put(&a->static_method_types, f, ttype, ahhd, ahed);

    return ast;
}

AST *ast_class_name(AST *ast, char *name)
{
    ASTClassDecl *a = (ASTClassDecl *)ast;
    a->name = name;

    return ast;
}

AST *ast_make_struct_get(AST *left, char *ident)
{
    ASTStructMemberGet *ast = ast_malloc(sizeof(ASTStructMemberGet));
    ast->type = ASTRUCTMEMBER;

    ast->left = left;
    ast->ident = ident;
    ast->leftCompiled = NULL;

    return (AST *)ast;
}

void ast_set_counted(AST *ast)
{
    ASTVarDecl *a = (ASTVarDecl *)ast;
    ASTTypeDecl *td = (ASTTypeDecl *)a->atype;
    if(td->etype->type != ETPointer)
        die(yylineno, msgerr_invalid_counted_type);
    EaglePointerType *pt = (EaglePointerType *)td->etype;
    pt->counted = 1;
}

AST *ast_make_arr_decl(AST *atype, char *ident, AST *expr)
{
    ASTVarDecl *ast = ast_malloc(sizeof(ASTVarDecl));
    ast->type = AVARDECL;
    ast->atype = atype;
    ast->ident = ident;
    ast->arrct = expr;
    ast->noSetNil = 0;
    ast->linkage = VLNone;
    ast->staticInit = NULL;

    return (AST *)ast;
}

void ast_set_linkage(AST *ast, VariableLinkage linkage)
{
    ASTVarDecl *a = (ASTVarDecl *)ast;
    a->linkage = linkage;
}

void ast_set_static_init(AST *ast, AST *staticInit)
{
    ASTVarDecl *a = (ASTVarDecl *)ast;
    a->staticInit = staticInit;
}

AST *ast_make_type(char *type)
{
    ASTTypeDecl *ast = ast_malloc(sizeof(ASTTypeDecl));
    ast->type = ATYPE;
    ast->etype = et_parse_string(type);

    return (AST *)ast;
}

AST *ast_make_generic_type(char *ident)
{
    ASTTypeDecl *ast = ast_malloc(sizeof(ASTTypeDecl));
    ast->type = ATYPE;

    ast->etype = ett_generic_type(ident);
    
    return (AST *)ast;
}

AST *ast_make_closure_type(AST *tysList, AST *resType)
{
    ASTTypeDecl *ast = ast_malloc(sizeof(ASTTypeDecl));
    ast->type = ATYPE;

    Arraylist list = arr_create(10);
    ASTVarDecl *vd = (ASTVarDecl *)tysList;
    for(; vd; vd = (ASTVarDecl *)vd->next)
    {
        ASTTypeDecl *t = (ASTTypeDecl *)vd->atype;
        arr_append(&list, t->etype);
    }

    ast->etype = ett_function_type(((ASTTypeDecl *)resType)->etype, (EagleComplexType **)list.items, list.count);
    ((EagleFunctionType *)ast->etype)->closure = CLOSURE_NO_CLOSE;

    arr_free(&list);

    return (AST *)ast;
}

AST *ast_make_function_type(AST *tysList, AST *resType)
{
    ASTTypeDecl *ast = ast_malloc(sizeof(ASTTypeDecl));
    ast->type = ATYPE;

    Arraylist list = arr_create(10);
    ASTVarDecl *vd = (ASTVarDecl *)tysList;
    for(; vd; vd = (ASTVarDecl *)vd->next)
    {
        ASTTypeDecl *t = (ASTTypeDecl *)vd->atype;
        arr_append(&list, t->etype);
    }

    ast->etype = ett_function_type(((ASTTypeDecl *)resType)->etype, (EagleComplexType **)list.items, list.count);
    ((EagleFunctionType *)ast->etype)->closure = 0;

    arr_free(&list);

    return (AST *)ast;
}

AST *ast_make_gen_type(AST *ytype)
{
    ASTTypeDecl *ast = ast_malloc(sizeof(ASTTypeDecl));
    ast->type = ATYPE;

    ast->etype = ett_gen_type(((ASTTypeDecl *)ytype)->etype);

    return (AST *)ast;
}

AST *ast_make_pointer(AST *ast)
{
    ASTTypeDecl *a = (ASTTypeDecl *)ast;

    if(a->etype->type == ETVoid)
        die(a->lineno, msgerr_void_ptr_decl);

    /*
    if(a->etype->type == ETPointer)
    {
        EaglePointerType *pt = (EaglePointerType *)a->etype;
        for(; pt->to->type == ETPointer; pt = (EaglePointerType *)pt->to);
        pt->to = ett_pointer_type(pt->to);
        return ast;
    }
    */
    a->etype = ett_pointer_type(a->etype);

    return ast;
}

AST *ast_make_counted(AST *ast)
{
    ASTTypeDecl *a = (ASTTypeDecl *)ast;

    if(a->etype->type != ETPointer)
        die(a->lineno, msgerr_invalid_counted_type);
    EaglePointerType *ep = (EaglePointerType *)a->etype;
    ep->counted = 1;

    return ast;
}

AST *ast_make_weak(AST *ast)
{
    ASTTypeDecl *a = (ASTTypeDecl *)ast;

    if(a->etype->type != ETPointer)
        die(a->lineno, msgerr_invalid_weak_type);
    EaglePointerType *ep = (EaglePointerType *)a->etype;
    if(!ep->counted)
        die(a->lineno, msgerr_invalid_weak_ptr_type);

    ep->weak = 1;
    ep->counted = 0;

    return ast;
}

AST *ast_make_composite(AST *orig, char *nw)
{
    ASTTypeDecl *a = (ASTTypeDecl *)orig;

    if(a->etype->type != ETInterface || !ty_is_interface(nw))
        die(a->lineno, msgerr_composite_from_non_interfaces);

    ett_composite_interface(a->etype, nw);

    return orig;
}

AST *ast_make_array(AST *ast, int ct)
{
    ASTTypeDecl *a = (ASTTypeDecl *)ast;

    if(a->etype->type == ETArray)
    {
        EagleArrayType *et = (EagleArrayType *)a->etype;
        for(; et->of->type == ETArray; et = (EagleArrayType *)et->of);
        et->of = ett_array_type(et->of, ct);
        return ast;
    }
    a->etype = ett_array_type(a->etype, ct);

    return ast;
}

AST *ast_make_if(AST *test, AST *block)
{
    ASTIfBlock *ast = ast_malloc(sizeof(ASTIfBlock));
    ast->type = AIF;

    ast->test = test;
    ast->block = block;
    ast->ifNext = NULL;

    return (AST *)ast;
}

AST *ast_make_switch(AST *test, AST *cases)
{
    ASTSwitchBlock *ast = ast_malloc(sizeof(ASTSwitchBlock));
    ast->type = ASWITCH;

    ast->test = test;
    ast->cases = cases;
    ast->default_index = -1;

    for(int i = 0; cases; cases = cases->next, i++)
    {
        ASTCaseBlock *cs = (ASTCaseBlock *)cases;
        if(!cs->targ) // Default value
        {
            if(ast->default_index >= 0) // We already have a default
                die(ast->lineno, msgerr_duplicate_default_case);
            ast->default_index = i;
        }
    }
    
    return (AST *)ast;
}

AST *ast_make_case(AST *targ, AST *body)
{
    ASTCaseBlock *ast = ast_malloc(sizeof(ASTCaseBlock));
    ast->type = ACASE;

    ast->targ = targ;
    ast->body = body;

    return (AST *)ast;
}

AST *ast_make_loop(AST *setup, AST *test, AST *update, AST *block)
{
    ASTLoopBlock *ast = ast_malloc(sizeof(ASTLoopBlock));
    ast->type = ALOOP;

    ast->setup = setup;
    ast->test = test;
    ast->update = update;
    ast->block = block;

    return (AST *)ast;
}

AST *ast_make_ternary(AST *test, AST *ifyes, AST *ifno)
{
    ASTTernary *ast = ast_malloc(sizeof(ASTTernary));
    ast->type = ATERNARY;

    ast->test  = test;
    ast->ifyes = ifyes;
    ast->ifno  = ifno;

    return (AST *)ast;
}

AST *ast_make_defer(AST *block)
{
    ASTDefer *ast = ast_malloc(sizeof(ASTDefer));
    ast->type = ADEFER;

    ast->block = block;

    return (AST *)ast;
}

AST *ast_make_cast(AST *type, AST *val)
{
    ASTCast *ast = ast_malloc(sizeof(ASTCast));
    ast->type = ACAST;

    ast->etype = type;
    ast->val = val;

    return (AST *)ast;
}

AST *ast_make_enum(char *type, AST *items)
{
    ASTEnumDecl *ast = ast_malloc(sizeof(ASTEnumDecl));
    ast->type = AENUMDECL;

    ast->name = type;
    ast->items = items;

    return (AST *)ast;
}

AST *ast_make_enumitem(char *name, char *def)
{
    ASTEnumItem *ast = ast_malloc(sizeof(ASTEnumItem));
    ast->type = AENUMITEM;

    ast->item = name;

    ast->def = def ? atol(def) : 0;
    ast->has_def = def ? 1 : 0;

    return (AST *)ast;
}

AST *ast_make_type_lookup(char *type, char *item)
{
    ASTTypeLookup *ast = ast_malloc(sizeof(ASTTypeLookup));
    ast->type = ATYPELOOKUP;

    ast->name = type;
    ast->item = item;

    return (AST *)ast;
}

AST *ast_make_export(char *text, int tok)
{
    ASTExportSymbol *ast = ast_malloc(sizeof(ASTExportSymbol));
    ast->type = AEXPORT;
    ast->kind = tok;

    ast->fmt = text + 1;
    text[strlen(text) - 1] = '\0';

    return (AST *)ast;
}

AST *ast_set_external_linkage(AST *ast)
{
    ASTLinkable *al = (ASTLinkable *)ast;
    al->linkage = VLExport;

    return ast;
}

void ast_add_if(AST *ast, AST *next)
{
    ASTIfBlock *i = (ASTIfBlock *)ast;
    for(; i->ifNext; i = (ASTIfBlock *)i->ifNext);
    i->ifNext = next;
}

