/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef AC_CLASS_H
#define AC_CLASS_H

typedef struct {
    CompilerBundle *cb;
    AST *ast;
    LLVMValueRef *interface_pointers;
    LLVMValueRef vtable;
    int table_len;

    VariableLinkage linkage;
} ClassHelper;

char *ac_gen_method_name(char *class_name, char *method);
char *ac_gen_static_method_name(char *class_name, char *method);
void ac_add_class_declaration(AST *ast, CompilerBundle *cb);
void ac_class_add_early_definitions(AST *ast, CompilerBundle *cb);
void ac_generate_interface_definitions(AST *ast, CompilerBundle *cb);
void ac_make_class_definitions(AST *ast, CompilerBundle *cb);
void ac_make_class_constructor(AST *ast, CompilerBundle *cb, ClassHelper *h);
void ac_make_class_destructor(AST *ast, CompilerBundle *cb);

#endif
