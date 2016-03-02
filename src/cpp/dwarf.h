/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef DWARF_H
#define DWARF_H

#include <llvm-c/Core.h>
#include "compiler/ast.h"
#ifdef __cplusplus

extern "C" {
#endif 

typedef struct DWBuilder DWBuilder;
typedef DWBuilder* DWBuilderRef;

DWBuilderRef DWInit(LLVMModuleRef mod, char *filename);
void DWAddFunction(DWBuilderRef br, LLVMValueRef func, AST *ast);
void DWFinalize(DWBuilderRef builder);

#ifdef __cplusplus
}
#endif
#endif
 
