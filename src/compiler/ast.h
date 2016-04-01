/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
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
    ASTRUCTLIT,
    ACLASSDECL,
    ACLASSMEMBER,
    AIFCDECL,
    AIDENT,
    AIF,
    ALOOP,
    ACAST,
    ATYPE,
    AALLOC,
    AENUMDECL,
    AENUMITEM,
    ATYPELOOKUP,
    AEXPORT,
    ASWITCH,
    ACASE
} ASTType;

typedef enum {
    VLNone,
    VLExternal,
    VLStatic,
    VLLocal,
    VLExport
} VariableLinkage;

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

    VariableLinkage linkage;
} ASTLinkable;

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
    VariableLinkage linkage;

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

    // Fields to manage global variables
    VariableLinkage linkage;
    AST *staticInit;

    // This field is *only* used for generator calls
    // when we want to avoid setting a parameter in
    // the generator context to nil...
    int noSetNil;
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

    struct AST *test;
    struct AST *cases;
    struct AST *deflt;
} ASTSwitchBlock;

typedef struct {
    ASTType type;
    EagleTypeType *resultantType;
    struct AST *next;
    long lineno;

    struct AST *targ;
    struct AST *body;
} ASTCaseBlock;

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
    VariableLinkage linkage;

    int ext;

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

    hashtable exprs;
    char *name;
} ASTStructLit;

typedef struct {
    ASTType type;
    EagleTypeType *resultantType;
    struct AST *next;
    long lineno;
    VariableLinkage linkage;

    char *name;
    arraylist types;
    arraylist names;

    struct AST *initdecl;
    struct AST *destructdecl;
    EagleTypeType *inittype;
    EagleTypeType *destructtype;

    arraylist interfaces;

    // hashtable methods;
    hashtable method_types;

    int ext;
} ASTClassDecl;

typedef struct {
    ASTType type;
    EagleTypeType *resultantType;
    struct AST *next;
    long lineno;
    VariableLinkage linkage;

    char *name;
    struct AST *items;
} ASTEnumDecl;

typedef struct {
    ASTType type;
    EagleTypeType *resultantType;
    struct AST *next;
    long lineno;

    char *item;
    long def;
    int has_def;
} ASTEnumItem;

typedef struct {
    ASTType type;
    EagleTypeType *resultantType;
    struct AST *next;
    long lineno;

    char *name;
    char *item;
} ASTTypeLookup;

typedef struct {
    ASTType type;
    EagleTypeType *resultantType;
    struct AST *next;
    long lineno;

    char *fmt;
    int kind;
} ASTExportSymbol;

AST *ast_make();
void ast_append(AST *old, AST *n);
AST *ast_make_binary(AST *left, AST *right, char op);
AST *ast_make_unary(AST *val, char op);
AST *ast_make_allocater(char op, AST *val, AST *init);
AST *ast_make_bool(int i);
AST *ast_make_nil();
AST *ast_make_int32(char *text);
AST *ast_make_byte(char *text);
AST *ast_make_double(char *text);
AST *ast_make_cstr(char *text);
AST *ast_make_identifier(char *ident);
AST *ast_set_vararg(AST *ast);
AST *ast_make_func_decl(AST *type, char *ident, AST *body, AST *params);
AST *ast_make_class_special_decl(char *ident, AST *body, AST *params);
AST *ast_make_gen_decl(AST *type, char *ident, AST *body, AST *params);
AST *ast_make_func_call(AST *callee, AST *params);
AST *ast_make_var_decl(AST *type, char *ident);
AST *ast_make_auto_decl(char *ident);
AST *ast_make_struct_decl();
AST *ast_struct_add(AST *ast, AST *var);
AST *ast_struct_name(AST *ast, char *name);
void ast_struct_set_extern(AST *ast);
AST *ast_make_struct_lit_dict();
void ast_struct_lit_add(void *dict, char *ident, AST *expr);
AST *ast_make_struct_lit(char *name, AST *dict);
AST *ast_make_class_decl();
AST *ast_make_interface_decl();
AST *ast_class_set_init(AST *cls, AST *init);
AST *ast_class_var_add(AST *ast, AST *var);
AST *ast_class_method_add(AST *ast, AST *func);
AST *ast_class_name(AST *ast, char *name);
void ast_class_set_extern(AST *ast);
void ast_class_add_interface(AST *ast, AST *interfaces);
AST *ast_make_struct_get(AST *left, char *ident);
void ast_set_counted(AST *ast);
AST *ast_make_arr_decl(AST *type, char *ident, AST *expr);
void ast_set_linkage(AST *ast, VariableLinkage linkage);
void ast_set_static_init(AST *ast, AST *staticInit);
AST *ast_make_type(char *type);
AST *ast_make_closure_type(AST *tysList, AST *resType);
AST *ast_make_function_type(AST *tysList, AST *resType);
AST *ast_make_gen_type(AST *ytype);
AST *ast_make_pointer(AST *ast);
AST *ast_make_counted(AST *ast);
AST *ast_make_weak(AST *ast);
AST *ast_make_composite(AST *orig, char *nw);
AST *ast_make_array(AST *ast, int ct);
AST *ast_make_if(AST *test, AST *block);
AST *ast_make_switch(AST *test, AST *cases);
AST *ast_make_case(AST *targ, AST *body);
AST *ast_make_loop(AST *setup, AST *test, AST *update, AST *block);
AST *ast_make_cast(AST *type, AST *val);
AST *ast_make_enum(char *type, AST *items);
AST *ast_make_enumitem(char *name, char *def);
AST *ast_make_type_lookup(char *type, char *item);
AST *ast_make_export(char *text, int tok);
AST *ast_set_external_linkage(AST *ast);
void ast_add_if(AST *ast, AST *next);

void ast_free_nodes();

#endif /* defined(__Eagle__ast__) */
