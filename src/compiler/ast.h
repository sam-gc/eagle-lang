//
//  ast.h
//  Eagle
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
    ATYPE
} ASTType;

typedef struct AST {
    ASTType type;
    EagleType resultantType;
    struct AST *next;
} AST;

typedef struct {
    ASTType type;
    EagleType resultantType;
    struct AST *next;
    
    struct AST *left;
    struct AST *right;
    char op;
} ASTBinary;

typedef struct {
    ASTType type;
    EagleType resultantType;
    struct AST *next;

    struct AST *val;
    char op;
} ASTUnary;

typedef struct {
    ASTType type;
    EagleType resultantType;
    struct AST *next;
    
    EagleType etype;
    union {
        int32_t i;
        double d;
        char *id;
    } value;
} ASTValue;

typedef struct {
    ASTType type;
    EagleType resultantType;
    struct AST *next;
    
    EagleType retType;
    struct AST *body;
    struct AST *params;
    char *ident;
} ASTFuncDecl;

typedef struct {
    ASTType type;
    EagleType resultantType;
    struct AST *next;

    struct AST *callee;
    struct AST *params;
} ASTFuncCall;

typedef struct {
    ASTType type;
    EagleType resultantType;
    struct AST *next;

    EagleType etype;
    char *ident;
} ASTVarDecl;

AST *ast_make();
void ast_append(AST *old, AST *n);
AST *ast_make_binary(AST *left, AST *right, char op);
AST *ast_make_unary(AST *val, char op);
AST *ast_make_int32(char *text);
AST *ast_make_double(char *text);
AST *ast_make_identifier(char *ident);
AST *ast_make_func_decl(char *type, char *ident, AST *body, AST *params);
AST *ast_make_func_call(AST *callee, AST *params);
AST *ast_make_var_decl(char *type, char *ident);

#endif /* defined(__Eagle__ast__) */
