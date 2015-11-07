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
#include "core/arraylist.h"
#include "core/hashtable.h"

typedef enum {
    ASKIP,
    ABASE,
    ABINARY,
    AUNARY,
    AVALUE,
    AFUNCDECL,
    AGENDECL,
    AFUNCCALL,
    AVARDECL,
    ASTRUCTDECL,
    ASTRUCTMEMBER,
    ACLASSDECL,
    ACLASSMEMBER,
    AIDENT,
    AIF,
    ALOOP,
    ACAST,
    ATYPE,
    AALLOC
} ASTType;

typedef struct AST {
    ASTType type;
    EagleTypeType *resultantType; struct AST *next;
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

    LLVMValueRef savedWrapped; // Used for classes
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
    int vararg;
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
    struct AST *arrct;
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

    struct AST *setup;
    struct AST *test;
    struct AST *update;
    struct AST *block;
} ASTLoopBlock;

typedef struct {
    ASTType type;
    EagleTypeType *resultantType;
    struct AST *next;
    long lineno;

    struct AST *etype;
    struct AST *val;
} ASTCast;

typedef struct {
    ASTType type;
    EagleTypeType *resultantType;
    struct AST *next;
    long lineno;

    char *name;
    arraylist types;
    arraylist names;
} ASTStructDecl;

typedef struct {
    ASTType type;
    EagleTypeType *resultantType;
    struct AST *next;
    long lineno;

    AST *left;
    char *ident;

    LLVMValueRef leftCompiled;
} ASTStructMemberGet;

typedef struct {
    ASTType type;
    EagleTypeType *resultantType;
    struct AST *next;
    long lineno;

    char *name;
    arraylist types;
    arraylist names;

    struct AST *initdecl;
    EagleTypeType *inittype;

    hashtable methods;
} ASTClassDecl;

AST *ast_make();
void ast_append(AST *old, AST *n);
AST *ast_make_binary(AST *left, AST *right, char op);
AST *ast_make_unary(AST *val, char op);
AST *ast_make_allocater(char op, AST *val, AST *init);
AST *ast_make_bool(int i);
AST *ast_make_nil();
AST *ast_make_int32(char *text);
AST *ast_make_double(char *text);
AST *ast_make_cstr(char *text);
AST *ast_make_identifier(char *ident);
AST *ast_set_vararg(AST *ast);
AST *ast_make_func_decl(AST *type, char *ident, AST *body, AST *params);
AST *ast_make_init_decl(char *ident, AST *body, AST *params);
AST *ast_make_gen_decl(AST *type, char *ident, AST *body, AST *params);
AST *ast_make_func_call(AST *callee, AST *params);
AST *ast_make_var_decl(AST *type, char *ident);
AST *ast_make_auto_decl(char *ident);
AST *ast_make_struct_decl();
AST *ast_struct_add(AST *ast, AST *var);
AST *ast_struct_name(AST *ast, char *name);
AST *ast_make_class_decl();
AST *ast_class_set_init(AST *cls, AST *init);
AST *ast_class_var_add(AST *ast, AST *var);
AST *ast_class_method_add(AST *ast, AST *func);
AST *ast_class_name(AST *ast, char *name);
AST *ast_make_struct_get(AST *left, char *ident);
void ast_set_counted(AST *ast);
AST *ast_make_arr_decl(AST *type, char *ident, AST *expr);
AST *ast_make_type(char *type);
AST *ast_make_closure_type(AST *tysList, AST *resType);
AST *ast_make_function_type(AST *tysList, AST *resType);
AST *ast_make_pointer(AST *ast);
AST *ast_make_counted(AST *ast);
AST *ast_make_weak(AST *ast);
AST *ast_make_array(AST *ast, int ct);
AST *ast_make_if(AST *test, AST *block);
AST *ast_make_loop(AST *setup, AST *test, AST *update, AST *block);
AST *ast_make_cast(AST *type, AST *val);
void ast_add_if(AST *ast, AST *next);

void ast_free_nodes();

#endif /* defined(__Eagle__ast__) */
