/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef AC_CONSTANTS_H
#define AC_CONSTANTS_H

LLVMValueRef ac_const_value(AST *ast, CompilerBundle *cb);
LLVMValueRef ac_convert_const(LLVMValueRef val, EagleComplexType *to, EagleComplexType *from);

#endif

