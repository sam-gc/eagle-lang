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

void ast_append(AST *old, AST *n)
{
    AST *prev = NULL;
    for(; old->next; old = old->next) prev = old;

    if(old->type == ASKIP)
    {
        if(prev)
        {
            prev->next = n;
        }
    }
    else
        old->next = n;
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

AST *ast_make_func_decl(char *type, char *ident, AST *body)
{
    ASTFuncDecl *ast = ast_malloc(sizeof(ASTFuncDecl));
    ast->type = AFUNCDECL;
    ast->retType = et_parse_string(type);
    ast->body = body;
    ast->ident = ident;
    
    return (AST *)ast;
}
