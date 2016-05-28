/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef AC_GENERAL_H
#define AC_GENERAL_H

#include "core/config.h"

typedef void (*DispatchObserver)(AST *ast, void *data);

LLVMModuleRef ac_compile(AST *ast, int include_rc);
void ac_prepare_module(LLVMModuleRef module);
void ac_add_early_name_declaration(AST *ast, CompilerBundle *cb);
void ac_add_global_variable_declarations(AST *ast, CompilerBundle *cb);
void ac_add_early_declarations(AST *ast, CompilerBundle *cb);
LLVMValueRef ac_dispatch_constant(AST *ast, CompilerBundle *cb);
LLVMValueRef ac_dispatch_expression(AST *ast, CompilerBundle *cb);
void ac_dispatch_statement(AST *ast, CompilerBundle *cb);
void ac_dispatch_declaration(AST *ast, CompilerBundle *cb);
void ac_flush_transients(CompilerBundle *cb);
void ac_guard_deferment(CompilerBundle *cb, int lineno);
void ac_add_dispatch_observer(CompilerBundle *cb, ASTType type, DispatchObserver observer, void *data);
void ac_remove_dispatch_observer(CompilerBundle *cb, ASTType type);

#endif
