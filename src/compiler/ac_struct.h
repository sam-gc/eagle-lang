/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef AC_STRUCT_H
#define AC_STRUCT_H

void ac_scope_leave_struct_callback(LLVMValueRef pos, EagleTypeType *ty, void *data);
LLVMValueRef ac_gen_struct_destructor_func(char *name, CompilerBundle *cb);
LLVMValueRef ac_gen_struct_constructor_func(char *name, CompilerBundle *cb, int copy);
void ac_make_struct_copy_constructor(AST *ast, CompilerBundle *cb);
void ac_make_struct_constructor(AST *ast, CompilerBundle *cb);
void ac_make_struct_destructor(AST *ast, CompilerBundle *cb);
void ac_add_struct_declaration(AST *ast, CompilerBundle *cb);
void ac_make_struct_definitions(AST *ast, CompilerBundle *cb);
void ac_call_destructor(CompilerBundle *cb, LLVMValueRef pos, EagleTypeType *ty);
void ac_call_constructor(CompilerBundle *cb, LLVMValueRef pos, EagleTypeType *ty);
void ac_call_copy_constructor(CompilerBundle *cb, LLVMValueRef pos, EagleTypeType *ty);
LLVMValueRef ac_compile_struct_lit(AST *ast, CompilerBundle *cb);

#endif
