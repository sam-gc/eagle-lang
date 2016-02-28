/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef AC_GENERATOR_H
#define AC_GENERATOR_H

#include "core/arraylist.h"
#include "core/hashtable.h"

typedef struct {
    arraylist *elems;
    hashtable *param_mapping;
    hashtable *params;
    char *ident;
    LLVMValueRef func;
    LLVMTypeRef contextType;
    LLVMTypeRef countedContextType;

    EagleTypeType **eparam_types;
    int epct;

    LLVMBasicBlockRef last_block;
} GeneratorBundle;

void ac_generator_replace_allocas(CompilerBundle *cb, GeneratorBundle *gb);
void ac_compile_generator_code(AST *ast, CompilerBundle *cb);
void ac_add_gen_declaration(AST *ast, CompilerBundle *cb);
void ac_null_out_counted(CompilerBundle *cb, GeneratorBundle *gb, LLVMValueRef btb);
LLVMValueRef ac_compile_generator_init(AST *ast, CompilerBundle *cb, GeneratorBundle *gb);

#endif
