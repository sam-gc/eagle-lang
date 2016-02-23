#ifndef AC_GENERAL_H
#define AC_GENERAL_H

#include "core/config.h"

LLVMModuleRef ac_compile(AST *ast, int include_rc);
void ac_prepare_module(LLVMModuleRef module);
void ac_add_early_name_declaration(AST *ast, CompilerBundle *cb);
void ac_add_early_declarations(AST *ast, CompilerBundle *cb);
LLVMValueRef ac_dispatch_expression(AST *ast, CompilerBundle *cb);
void ac_dispatch_statement(AST *ast, CompilerBundle *cb);
void ac_dispatch_declaration(AST *ast, CompilerBundle *cb);

#endif
