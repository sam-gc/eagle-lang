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
