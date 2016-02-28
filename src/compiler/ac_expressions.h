/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef AC_EXPRESSIONS_H
#define AC_EXPRESSIONS_H

LLVMValueRef ac_compile_value(AST *ast, CompilerBundle *cb);
LLVMValueRef ac_compile_identifier(AST *ast, CompilerBundle *cb);
LLVMValueRef ac_compile_var_decl(AST *ast, CompilerBundle *cb);
LLVMValueRef ac_compile_var_decl_ext(EagleTypeType *type, char *ident, CompilerBundle *cb, int noSetNil);
LLVMValueRef ac_compile_struct_member(AST *ast, CompilerBundle *cb, int keepPointer);
LLVMValueRef ac_compile_type_lookup(AST *ast, CompilerBundle *cb);
LLVMValueRef ac_compile_malloc_counted_raw(LLVMTypeRef rt, LLVMTypeRef *out, CompilerBundle *cb);
LLVMValueRef ac_compile_malloc_counted(EagleTypeType *type, EagleTypeType **res, LLVMValueRef ib, CompilerBundle *cb);
LLVMValueRef ac_compile_new_decl(AST *ast, CompilerBundle *cb);
LLVMValueRef ac_compile_cast(AST *ast, CompilerBundle *cb);
LLVMValueRef ac_compile_index(AST *ast, int keepPointer, CompilerBundle *cb);
LLVMValueRef ac_compile_binary(AST *ast, CompilerBundle *cb);
LLVMValueRef ac_compile_get_address(AST *of, CompilerBundle *cb);
LLVMValueRef ac_compile_unary(AST *ast, CompilerBundle *cb);
LLVMValueRef ac_compile_generator_call(AST *ast, LLVMValueRef gen, CompilerBundle *cb);
LLVMValueRef ac_compile_function_call(AST *ast, CompilerBundle *cb);
LLVMValueRef ac_build_store(AST *ast, CompilerBundle *cb, char update);
LLVMValueRef ac_generic_unary(ASTUnary *a, LLVMValueRef val, CompilerBundle *cb);

#endif
