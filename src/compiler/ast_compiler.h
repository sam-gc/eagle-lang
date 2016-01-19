#ifndef AST_COMPILER_H
#define AST_COMPILER_H

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "core/llvm_headers.h"
#include "ast.h"
#include "variable_manager.h"
#include "cpp/cpp.h"
#include "core/utils.h"

#define IS_ANY_PTR(t) (t->type == ETPointer && ((EaglePointerType *)t)->to->type == ETAny)
#define ALN (ast->lineno)
#define LN(a) (a->lineno)

typedef struct {
    LLVMModuleRef module;
    LLVMBuilderRef builder;

    LLVMTargetDataRef td;

    EagleFunctionType *currentFunctionType;
    EagleGenType *currentGenType;

    arraylist *yieldBlocks;
    LLVMBasicBlockRef currentYield;
    LLVMBasicBlockRef currentFunctionEntry;
    LLVMBasicBlockRef currentLoopEntry;
    LLVMBasicBlockRef currentLoopExit;
    LLVMValueRef currentFunction;
    VarScope *currentFunctionScope;

    VarScopeStack *varScope;
    hashtable transients;
    hashtable loadedTransients;

    int compilingMethod;
} CompilerBundle;

#include "ac_control_flow.h"
#include "ac_expressions.h"
#include "ac_functions.h"
#include "ac_general.h"
#include "ac_helpers.h"
#include "ac_rc.h"
#include "ac_struct.h"
#include "ac_class.h"
#include "ac_generator.h"

#endif
