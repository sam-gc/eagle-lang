#ifndef AST_COMPILER_H
#define AST_COMPILER_H

#include "core/llvm_headers.h"
#include "ast.h"

LLVMModuleRef ac_compile(AST *ast);

#endif
