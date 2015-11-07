#ifndef AC_CLASS_H
#define AC_CLASS_H

typedef struct {
    CompilerBundle *cb;
    AST *ast;
} ac_class_helper;

char *ac_gen_method_name(char *class_name, char *method);
void ac_add_class_declaration(AST *ast, CompilerBundle *cb);
void ac_make_class_definitions(AST *ast, CompilerBundle *cb);
void ac_make_class_constructor(AST *ast, CompilerBundle *cb);
void ac_make_class_destructor(AST *ast, CompilerBundle *cb);

#endif
