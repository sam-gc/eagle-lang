#ifndef AC_CONTROL_FLOW_H
#define AC_CONTROL_FLOW_H

int ac_compile_block(AST *ast, LLVMBasicBlockRef block, CompilerBundle *cb);
void ac_compile_if(AST *ast, CompilerBundle *cb, LLVMBasicBlockRef mergeBB);
void ac_compile_loop(AST *ast, CompilerBundle *cb);
LLVMValueRef ac_compile_test(AST *res, LLVMValueRef val, CompilerBundle *cb);

#endif