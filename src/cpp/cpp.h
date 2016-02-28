/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CPP_H
#define CPP_H

#include <llvm-c/Core.h>

#ifdef __cplusplus
extern "C" {
#endif

LLVMValueRef EGLBuildMalloc(LLVMBuilderRef B, LLVMTypeRef Ty, LLVMValueRef Before, const char *Name);
void EGLEraseFunction(LLVMValueRef func);
// void EGLGenerateAssembly(LLVMModuleRef module, char *filename);

#ifdef __cplusplus
}
#endif

#endif
