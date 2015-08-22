#ifndef AST_COMPILER_H
#define AST_COMPILER_H

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "core/llvm_headers.h"
#include "ast.h"
#include "variable_manager.h"
#include "cpp/cpp.h"

#define IS_ANY_PTR(t) (t->type == ETPointer && ((EaglePointerType *)t)->to->type == ETAny)
#define ALN (ast->lineno)
#define LN(a) (a->lineno)

typedef struct {
    LLVMModuleRef module;
    LLVMBuilderRef builder;

    EagleFunctionType *currentFunctionType;
    LLVMBasicBlockRef currentFunctionEntry;
    LLVMValueRef currentFunction;
    VarScope *currentFunctionScope;

    VarScopeStack *varScope;
    hashtable transients;
    hashtable loadedTransients;
} CompilerBundle;

#include "ac_control_flow.h"
#include "ac_expressions.h"
#include "ac_functions.h"
#include "ac_general.h"
#include "ac_helpers.h"
#include "ac_rc.h"
#include "ac_struct.h"

#endif
