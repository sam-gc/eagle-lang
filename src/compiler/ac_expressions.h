#ifndef AC_EXPRESSIONS_H
#define AC_EXPRESSIONS_H

LLVMValueRef ac_compile_value(AST *ast, CompilerBundle *cb);
LLVMValueRef ac_compile_identifier(AST *ast, CompilerBundle *cb);
LLVMValueRef ac_compile_var_decl(AST *ast, CompilerBundle *cb);
LLVMValueRef ac_compile_struct_member(AST *ast, CompilerBundle *cb, int keepPointer);
LLVMValueRef ac_compile_malloc_counted_raw(LLVMTypeRef rt, LLVMTypeRef *out, CompilerBundle *cb);
LLVMValueRef ac_compile_malloc_counted(EagleTypeType *type, EagleTypeType **res, LLVMValueRef ib, CompilerBundle *cb);
LLVMValueRef ac_compile_new_decl(AST *ast, CompilerBundle *cb);
LLVMValueRef ac_compile_cast(AST *ast, CompilerBundle *cb);
LLVMValueRef ac_compile_index(AST *ast, int keepPointer, CompilerBundle *cb);
LLVMValueRef ac_compile_binary(AST *ast, CompilerBundle *cb);
LLVMValueRef ac_compile_get_address(AST *of, CompilerBundle *cb);
LLVMValueRef ac_compile_unary(AST *ast, CompilerBundle *cb);
LLVMValueRef ac_compile_function_call(AST *ast, CompilerBundle *cb);
LLVMValueRef ac_build_store(AST *ast, CompilerBundle *cb);

#endif
