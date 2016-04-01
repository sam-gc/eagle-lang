/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef AC_CONTROL_FLOW_H
#define AC_CONTROL_FLOW_H

int ac_compile_block(AST *ast, LLVMBasicBlockRef block, CompilerBundle *cb);
void ac_compile_return(AST *ast, LLVMBasicBlockRef block, CompilerBundle *cb);
void ac_compile_yield(AST *ast, LLVMBasicBlockRef block, CompilerBundle *cb);
void ac_compile_if(AST *ast, CompilerBundle *cb, LLVMBasicBlockRef mergeBB);
void ac_compile_switch(AST *ast, CompilerBundle *cb);
void ac_compile_loop(AST *ast, CompilerBundle *cb);
LLVMValueRef ac_compile_ntest(AST *res, LLVMValueRef val, CompilerBundle *cb);
LLVMValueRef ac_compile_test(AST *res, LLVMValueRef val, CompilerBundle *cb);

#endif
