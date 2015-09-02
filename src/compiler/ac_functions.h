#ifndef AC_FUNCTIONS_H
#define AC_FUNCTIONS_H

typedef struct {
    char *name;
    CompilerBundle *cb;
    LLVMValueRef context;

    arraylist *contextTypes;
    arraylist *contextVals;
    arraylist *outerContextVals;

    LLVMTypeRef contextType;

    LLVMBasicBlockRef cfib;
    LLVMBasicBlockRef entry;
    LLVMValueRef function;
    LLVMTypeRef funcType;

    LLVMValueRef outerContext;
} ClosureBundle;

char *ac_closure_context_name(char *name);
char *ac_closure_code_name(char *name);
char *ac_closure_closure_name(char *name);
char *ac_closure_destructor_name(char *name);
void ac_pre_prepare_closure(CompilerBundle *cb, char *name, ClosureBundle *bun);
LLVMValueRef ac_finish_closure(CompilerBundle *cb, ClosureBundle *bun, LLVMTypeRef *storageType);
void ac_closure_callback(VarBundle *vb, char *ident, void *data);
LLVMValueRef ac_compile_closure(AST *ast, CompilerBundle *cb);
void ac_compile_function(AST *ast, CompilerBundle *cb);

#endif
