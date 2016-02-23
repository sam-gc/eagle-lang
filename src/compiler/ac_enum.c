#include "ast_compiler.h"

void ac_make_enum_definitions(AST *ast, CompilerBundle *cb)
{
    for(; ast; ast = ast->next)
    {
        if(ast->type == AENUMDECL)
            ac_compile_enum_decleration(ast, cb);
    }
}

void ac_compile_enum_decleration(AST *ast, CompilerBundle *cb)
{
    ASTEnumDecl *a = (ASTEnumDecl *)ast;
    char *enum_name = a->name;

    ASTEnumItem *item = (ASTEnumItem *)a->items;

    long counter = 0;
    for(; item; item = (ASTEnumItem *)item->next)
    {
        ty_add_enum_item(enum_name, item->item, counter++);
    }
}

