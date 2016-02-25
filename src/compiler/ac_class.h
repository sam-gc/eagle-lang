#ifndef AC_CLASS_H
#define AC_CLASS_H

typedef struct {
    CompilerBundle *cb;
    AST *ast;
    LLVMValueRef *interface_pointers;
    LLVMValueRef vtable;
    int table_len;
} ac_class_helper;

char *ac_gen_method_name(char *class_name, char *method);
void ac_add_class_declaration(AST *ast, CompilerBundle *cb);
void ac_class_add_early_definitions(ASTClassDecl *cd, CompilerBundle *cb, ac_class_helper *h);
void ac_generate_interface_definitions(AST *ast, CompilerBundle *cb);
void ac_make_class_definitions(AST *ast, CompilerBundle *cb);
void ac_make_class_constructor(AST *ast, CompilerBundle *cb, ac_class_helper *h);
void ac_make_class_destructor(AST *ast, CompilerBundle *cb);

#endif
