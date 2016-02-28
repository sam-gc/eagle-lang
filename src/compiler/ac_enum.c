/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

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
        if(item->has_def)
            counter = item->def;

        ty_add_enum_item(enum_name, item->item, counter++);
    }
}
