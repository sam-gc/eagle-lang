/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef AC_GENERICS_H
#define AC_GENERICS_H

int ac_decl_is_generic(AST *ast);
void ac_generic_register(AST *ast, EagleComplexType *template_type, CompilerBundle *cb);
void ac_compile_generics(CompilerBundle *cb);
LLVMValueRef ac_generic_get(char *func, EagleComplexType *arguments[], EagleComplexType **out_type, CompilerBundle *cb, int lineno);
void ac_generics_cleanup(CompilerBundle *cb);

static inline int ac_is_generic(char *ident, CompilerBundle *cb)
{
    return hst_get(&cb->genericFunctions, ident, NULL, NULL) != NULL;
}

#endif

