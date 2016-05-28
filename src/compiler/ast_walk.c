#include <stdlib.h>
#include <string.h>
#include "ast_walk.h"
#include "core/hashtable.h"

#define CAST(type, tree) ((type *)(tree))

typedef struct
{
    ASTWalkOnType func;
    void *data;
} AWBundle;

struct ASTWalker
{
    AWBundle *ruleQuickDispatch[AFINALPLACEHOLDER];
};

static void aw_walk_all(ASTWalker *w, AST *ast);

static AWBundle *aw_make_bundle(ASTWalkOnType func, void *data)
{
    AWBundle *bun = malloc(sizeof(AWBundle));
    *bun = (AWBundle) {
        .func = func,
        .data = data
    };

    return bun;
}

static inline void aw_safe_dispatch(ASTWalker *w, AST *tree)
{
    if(tree)
        aw_walk_all(w, tree);
}

static void aw_hst_each_val(void *key, void *val, void *data)
{
    aw_walk(data, val);
}

static void aw_hst_each_key(void *key, void *val, void *data)
{
    aw_walk(data, key);
}

static void aw_dispatch(ASTWalker *w, AST *ast)
{
    AWBundle *impl = w->ruleQuickDispatch[ast->type];
    if(impl)
        impl->func(ast, impl->data);

    switch(ast->type)
    {
        case ABINARY:
            aw_safe_dispatch(w, CAST(ASTBinary, ast)->left);
            aw_safe_dispatch(w, CAST(ASTBinary, ast)->right);
            break;
        case AUNARY:
            aw_safe_dispatch(w, CAST(ASTUnary, ast)->val);
            break;
        case AFUNCDECL:
            aw_safe_dispatch(w, CAST(ASTFuncDecl, ast)->retType);
            aw_safe_dispatch(w, CAST(ASTFuncDecl, ast)->body);
            aw_safe_dispatch(w, CAST(ASTFuncDecl, ast)->params);
            break;
        case AFUNCCALL:
            aw_safe_dispatch(w, CAST(ASTFuncCall, ast)->callee);
            aw_safe_dispatch(w, CAST(ASTFuncCall, ast)->params);
            break;
        case AVARDECL:
            aw_safe_dispatch(w, CAST(ASTVarDecl, ast)->atype);
            aw_safe_dispatch(w, CAST(ASTVarDecl, ast)->arrct);
            aw_safe_dispatch(w, CAST(ASTVarDecl, ast)->staticInit);
            break;
        case AIF:
            aw_safe_dispatch(w, CAST(ASTIfBlock, ast)->test);
            aw_safe_dispatch(w, CAST(ASTIfBlock, ast)->block);
            aw_safe_dispatch(w, CAST(ASTIfBlock, ast)->ifNext);
            break;
        case ASWITCH:
            aw_safe_dispatch(w, CAST(ASTSwitchBlock, ast)->test);
            aw_safe_dispatch(w, CAST(ASTSwitchBlock, ast)->cases);
            break;
        case ACASE:
            aw_safe_dispatch(w, CAST(ASTCaseBlock, ast)->targ);
            aw_safe_dispatch(w, CAST(ASTCaseBlock, ast)->body);
            break;
        case ALOOP:
            aw_safe_dispatch(w, CAST(ASTLoopBlock, ast)->setup);
            aw_safe_dispatch(w, CAST(ASTLoopBlock, ast)->test);
            aw_safe_dispatch(w, CAST(ASTLoopBlock, ast)->update);
            aw_safe_dispatch(w, CAST(ASTLoopBlock, ast)->block);
            break;
        case ATERNARY:
            aw_safe_dispatch(w, CAST(ASTTernary, ast)->test);
            aw_safe_dispatch(w, CAST(ASTTernary, ast)->ifyes);
            aw_safe_dispatch(w, CAST(ASTTernary, ast)->ifno);
            break;
        case ACAST:
            aw_safe_dispatch(w, CAST(ASTCast, ast)->etype);
            aw_safe_dispatch(w, CAST(ASTCast, ast)->val);
            break;
        case ASTRUCTMEMBER:
            aw_safe_dispatch(w, CAST(ASTStructMemberGet, ast)->left);
            break;
        case ASTRUCTLIT:
            hst_for_each(&CAST(ASTStructLit, ast)->exprs, aw_hst_each_val, w);
            break;
        case ACLASSDECL:
            aw_safe_dispatch(w, CAST(ASTClassDecl, ast)->initdecl);
            aw_safe_dispatch(w, CAST(ASTClassDecl, ast)->destructdecl);
            hst_for_each(&CAST(ASTClassDecl, ast)->method_types, aw_hst_each_key, w);
            break;
        case AENUMDECL:
            aw_safe_dispatch(w, CAST(ASTEnumDecl, ast)->items);
            break;
        case ADEFER:
            aw_safe_dispatch(w, CAST(ASTDefer, ast)->block);
            break;
        default:
            break;
    }
}

static void aw_walk_all(ASTWalker *w, AST *ast)
{
    for(; ast; ast = ast->next)
        aw_dispatch(w, ast);
}

void aw_register(ASTWalker *w, ASTType type, ASTWalkOnType func, void *data)
{
    w->ruleQuickDispatch[type] = aw_make_bundle(func, data);
}

void aw_unregister(ASTWalker *w, ASTType type)
{
    if(w->ruleQuickDispatch[type])
    {
        free(w->ruleQuickDispatch[type]);
        w->ruleQuickDispatch[type] = NULL;
    }
}

void aw_walk(ASTWalker *w, AST *ast)
{
    aw_dispatch(w, ast);
}

ASTWalker *aw_create()
{
    ASTWalker *w = malloc(sizeof(ASTWalker));
    memset(w->ruleQuickDispatch, 0, sizeof(AWBundle *) * AFINALPLACEHOLDER);

    return w;
}

void aw_free(ASTWalker *w)
{
    for(int i = 0; i < AFINALPLACEHOLDER; i++)
        if(w->ruleQuickDispatch[i])
            free(w->ruleQuickDispatch[i]);
    free(w);
}

