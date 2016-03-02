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
 
