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

int yyerror(char *text)
{
    fprintf(stderr, "Error: %s\n", text);
    return -1;
}

void *ast_malloc(size_t size)
{
    AST *ast = malloc(size);
    ast->next = NULL;
    
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

AST *ast_make_func_decl(char *type, char *ident, AST *body, AST *params)
{
    ASTFuncDecl *ast = ast_malloc(sizeof(ASTFuncDecl));
    ast->type = AFUNCDECL;
    ast->retType = et_parse_string(type);
    ast->body = body;
    ast->ident = ident;
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

AST *ast_make_var_decl(char *type, char *ident)
{
    ASTVarDecl *ast = ast_malloc(sizeof(ASTVarDecl));
    ast->type = AVARDECL;
    ast->etype = et_parse_string(type);
    ast->ident = ident;

    return (AST *)ast;
}
