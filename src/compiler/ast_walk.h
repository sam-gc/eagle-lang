#ifndef AST_WALK_H
#define AST_WALK_H

#include "ast.h"

typedef struct ASTWalker ASTWalker;
typedef void (*ASTWalkOnType)(AST *ast, void *data);

void aw_register(ASTWalker *w, ASTType type, ASTWalkOnType func, void *data);
void aw_unregister(ASTWalker *w, ASTType type);
void aw_walk(ASTWalker *w, AST *ast);
ASTWalker *aw_create();
void aw_free(ASTWalker *w);

#endif

