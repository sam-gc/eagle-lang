//
//  ast.h //  Eagle
//
//  Created by Sam Olsen on 7/22/15.
//  Copyright (c) 2015 Sam Olsen. All rights reserved.
//

#ifndef __Eagle__ast__
#define __Eagle__ast__

#include <stdio.h>
#include <stdint.h>
#include "core/types.h"

typedef enum {
    ASKIP,
    ABASE,
    ABINARY,
    AUNARY,
    AVALUE,
    AFUNCDECL,
    AFUNCCALL,
    AVARDECL,
    AIDENT,
    AIF,
    ACAST,
    ATYPE
} ASTType;

typedef struct AST {
    ASTType type;
    EagleTypeType *resultantType;
    struct AST *next;
    long lineno;
} AST;

typedef struct {
    ASTType type;
    EagleTypeType *resultantType;
    struct AST *next;
    long lineno;
    
    struct AST *left;
    struct AST *right;
    char op;
} ASTBinary;

typedef struct {
    ASTType type;
    EagleTypeType *resultantType;
    struct AST *next;
    long lineno;

    struct AST *val;
    char op;
} ASTUnary;

typedef struct {
    ASTType type;
    EagleTypeType *resultantType;
    struct AST *next;
    long lineno;
    
    EagleType etype;
    union {
        int32_t i;
        double d;
        char *id;
    } value;
} ASTValue;

typedef struct {
    ASTType type;
    EagleTypeType *resultantType;
    struct AST *next;
    long lineno;
    
    struct AST *retType;
    struct AST *body;
    struct AST *params;
    char *ident;
} ASTFuncDecl;

typedef struct {
    ASTType type;
    EagleTypeType *resultantType;
    struct AST *next;
    long lineno;

    struct AST *callee;
    struct AST *params;
} ASTFuncCall;

typedef struct {
    ASTType type;
    EagleTypeType *resultantType;
    struct AST *next;
    long lineno;

    struct AST *atype;
    char *ident;
} ASTVarDecl;

typedef struct {
    ASTType type;
    EagleTypeType *resultantType;
    struct AST *next;
    long lineno;

    EagleTypeType *etype;
} ASTTypeDecl;

typedef struct {
    ASTType type;
    EagleTypeType *resultantType;
    struct AST *next;
    long lineno;

    struct AST *test;
    struct AST *block;
    struct AST *ifNext;
} ASTIfBlock;

typedef struct {
    ASTType type;
    EagleTypeType *resultantType;
    struct AST *next;
    long lineno;

    struct AST *etype;
    struct AST *val;
} ASTCast;

AST *ast_make();
void ast_append(AST *old, AST *n);
AST *ast_make_binary(AST *left, AST *right, char op);
AST *ast_make_unary(AST *val, char op);
AST *ast_make_int32(char *text);
AST *ast_make_double(char *text);
AST *ast_make_identifier(char *ident);
AST *ast_make_func_decl(AST *type, char *ident, AST *body, AST *params);
AST *ast_make_func_call(AST *callee, AST *params);
AST *ast_make_var_decl(AST *type, char *ident);
AST *ast_make_type(char *type);
AST *ast_make_pointer(AST *ast);
AST *ast_make_if(AST *test, AST *block);
AST *ast_make_cast(AST *type, AST *val);
void ast_add_if(AST *ast, AST *next);

#endif /* defined(__Eagle__ast__) */
