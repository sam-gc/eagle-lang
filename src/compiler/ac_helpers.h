/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef AC_HELPERS_H
#define AC_HELPERS_H

#define STRICT_CONVERSION 0
#define LOOSE_CONVERSION 1

long ahhd(void *k, void *d);
int ahed(void *k, void *d);
LLVMValueRef ac_make_floating_string(CompilerBundle *cb, const char *text, const char *name);
LLVMValueRef ac_build_conversion(CompilerBundle *cb, LLVMValueRef val, EagleTypeType *from, EagleTypeType *to, int try_view, int lineno);
LLVMValueRef ac_try_view_conversion(CompilerBundle *cb, LLVMValueRef val, EagleTypeType *from, EagleTypeType *to);
LLVMValueRef ac_make_add(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type, int lineno);
LLVMValueRef ac_make_sub(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type, int lineno);
LLVMValueRef ac_make_mul(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type, int lineno);
LLVMValueRef ac_make_div(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type, int lineno);
LLVMValueRef ac_make_mod(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type, int lineno);
LLVMValueRef ac_make_comp(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type, char comp, int lineno);
LLVMValueRef ac_make_neg(LLVMValueRef val, LLVMBuilderRef builder, EagleType type, int lineno);
void ac_replace_with_counted(CompilerBundle *cb, VarBundle *b);

#endif
