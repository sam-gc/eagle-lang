//
//  ast.c
//  Eagle
//
//  Created by Sam Olsen on 7/22/15.
//  Copyright (c) 2015 Sam Olsen. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include "ast.h"
#include "ast_compiler.h"
#include "core/arraylist.h"

extern int yylineno;
int yyerror(const char *text)
{
    fprintf(stderr, "%d: Error: %s\n", yylineno, text);
    return -1;
}

void *ast_malloc(size_t size)
{
    AST *ast = malloc(size);
    ast->next = NULL;
    ast->lineno = yylineno;
    
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

AST *ast_make_allocater(char op, AST *val)
{
    AST *ast = ast_make_unary(val, op);
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
    ast->etype = ETInt32;
    ast->value.i = atoi(text);
    
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

AST *ast_make_identifier(char *ident)
{
    ASTValue *ast = ast_malloc(sizeof(ASTValue));
    ast->type = AIDENT;
    ast->etype = ETNone;
    ast->value.id = ident;

    return (AST *)ast;
}

AST *ast_make_func_decl(AST *type, char *ident, AST *body, AST *params)
{
    ASTFuncDecl *ast = ast_malloc(sizeof(ASTFuncDecl));
    ast->type = AFUNCDECL;
    ast->retType = type;
    ast->body = body;
    ast->ident = ident ? ident : "close";
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

AST *ast_make_struct_decl()
{
    ASTStructDecl *ast = ast_malloc(sizeof(ASTStructDecl));
    ast->type = ASTRUCTDECL;

    ast->name = NULL;
    ast->names = arr_create(10);
    ast->types = arr_create(10);

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

AST *ast_make_struct_get(AST *left, char *ident)
{
    ASTStructMemberGet *ast = ast_malloc(sizeof(ASTStructMemberGet));
    ast->type = ASTRUCTMEMBER;

    ast->left = left;
    ast->ident = ident;

    return (AST *)ast;
}

void ast_set_counted(AST *ast)
{
    ASTVarDecl *a = (ASTVarDecl *)ast;
    ASTTypeDecl *td = (ASTTypeDecl *)a->atype;
    if(td->etype->type != ETPointer)
        die(yylineno, "Only pointer types can be counted.");
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

    return (AST *)ast;
}

AST *ast_make_type(char *type)
{
    ASTTypeDecl *ast = ast_malloc(sizeof(ASTTypeDecl));
    ast->type = ATYPE;
    ast->etype = et_parse_string(type);

    return (AST *)ast;
}

AST *ast_make_closure_type(AST *tysList, AST *resType)
{
    ASTTypeDecl *ast = ast_malloc(sizeof(ASTTypeDecl));
    ast->type = ATYPE;

    arraylist list = arr_create(10);
    ASTVarDecl *vd = (ASTVarDecl *)tysList;
    for(; vd; vd = (ASTVarDecl *)vd->next)
    {
        ASTTypeDecl *t = (ASTTypeDecl *)vd->atype;
        arr_append(&list, t->etype);
    }

    ast->etype = ett_function_type(((ASTTypeDecl *)resType)->etype, (EagleTypeType **)list.items, list.count);
    ((EagleFunctionType *)ast->etype)->closure = CLOSURE_NO_CLOSE;

    return (AST *)ast;
}

AST *ast_make_pointer(AST *ast)
{
    ASTTypeDecl *a = (ASTTypeDecl *)ast;

    if(a->etype->type == ETVoid)
        die(a->lineno, "Cannot declare void pointer type. Use an any-pointer instead (any *).");

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
        die(a->lineno, "Only pointer types may be counted.");
    EaglePointerType *ep = (EaglePointerType *)a->etype;
    ep->counted = 1;

    return ast;
}

AST *ast_make_weak(AST *ast)
{
    ASTTypeDecl *a = (ASTTypeDecl *)ast;

    if(a->etype->type != ETPointer)
        die(a->lineno, "Only pointer types may be declared weak.");
    EaglePointerType *ep = (EaglePointerType *)a->etype;
    if(!ep->counted)
        die(a->lineno, "Only counted pointers may be declared weak.");

    ep->weak = 1;
    ep->counted = 0;

    return ast;
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

AST *ast_make_cast(AST *type, AST *val)
{
    ASTCast *ast = ast_malloc(sizeof(ASTCast));
    ast->type = ACAST;

    ast->etype = type;
    ast->val = val;

    return (AST *)ast;
}

void ast_add_if(AST *ast, AST *next)
{
    ASTIfBlock *i = (ASTIfBlock *)ast;
    for(; i->ifNext; i = (ASTIfBlock *)i->ifNext);
    i->ifNext = next;
}

