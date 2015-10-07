#ifndef AC_CLASS_H
#define AC_CLASS_H

typedef struct {
    CompilerBundle *cb;
    AST *ast;
} ac_class_helper;

void ac_add_class_declaration(AST *ast, CompilerBundle *cb);
void ac_make_class_definitions(AST *ast, CompilerBundle *cb);
void ac_make_class_constructor(AST *ast, CompilerBundle *cb);
void ac_make_class_destructor(AST *ast, CompilerBundle *cb);

#endif
