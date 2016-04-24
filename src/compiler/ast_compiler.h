/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

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
#include "environment/exports.h"
#include "grammar/eagle.tab.h"

#define IS_ANY_PTR(t) (t->type == ETPointer && ((EaglePointerType *)t)->to->type == ETAny)
#define ALN (ast->lineno)
#define LN(a) (a->lineno)

typedef struct {
    LLVMModuleRef module;
    LLVMBuilderRef builder;

    LLVMTargetDataRef td;

    EagleFunctionType *currentFunctionType;
    EagleGenType *currentGenType;

    Arraylist *yieldBlocks;
    LLVMBasicBlockRef nextCaseBlock;
    LLVMBasicBlockRef currentYield;
    LLVMBasicBlockRef currentFunctionEntry;
    LLVMBasicBlockRef currentLoopEntry;
    LLVMBasicBlockRef currentLoopExit;
    LLVMValueRef currentFunction;
    VarScope *currentFunctionScope;
    VarScope *currentLoopScope;
    VarScope *currentCaseScope;

    VarScopeStack *varScope;
    Hashtable transients;
    Hashtable loadedTransients;

    int compilingMethod;
    int inDeferment;
    EagleComplexType *enum_lookup;

    ExportControl *exports;
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
#include "ac_enum.h"
#include "ac_constants.h"

#endif
