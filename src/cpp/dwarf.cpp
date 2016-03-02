/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdlib.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/LLVMContext.h>
#include "dwarf.h"

extern "C" {
#include "core/types.h"
}

using namespace llvm;

#define AST_FUNC(ast) ((ASTFuncDecl *)ast)

extern char *current_file_name;
DISubroutineType *DWFunctionType(DWBuilderRef br, EagleTypeType *ty);
DIType *DWEagleToDIType(DWBuilderRef br, EagleTypeType *ty);

struct DWBuilder
{
    DIBuilder *dib;
    DICompileUnit *cu;
    DIFile *unit;
};

DWBuilderRef DWInit(LLVMModuleRef mod, char *filename)
{
    DWBuilderRef builder = (DWBuilderRef)malloc(sizeof(DWBuilder));
    unwrap(mod);

    DIBuilder *dib = new DIBuilder(*unwrap(mod));
    builder->dib = dib;

    builder->cu = dib->createCompileUnit(
            dwarf::DW_LANG_C, filename, ".", "Eagle Compiler", 0, "", 0);

    builder->unit = dib->createFile(builder->cu->getFilename(),
            builder->cu->getDirectory());

    return builder;
}

void DWAddFunction(DWBuilderRef br, LLVMValueRef func, AST *ast)
{
    if(!br)
        return;

    EagleFunctionType *ft = (EagleFunctionType *)ast->resultantType;
    ASTFuncDecl *a = AST_FUNC(ast);
    DIScope *fctx = br->unit;
    DISubprogram *sp = br->dib->createFunction(
            fctx, a->ident, StringRef(), br->unit, a->lineno,
            DWFunctionType(br, ast->resultantType), true, true,
            a->lineno, DINode::FlagPrototyped, false);
    unwrap<Function>(func)->setMetadata(LLVMContext::MD_dbg, sp);
}

void DWFinalize(DWBuilderRef builder)
{
    builder->dib->finalize();
}

DISubroutineType *DWFunctionType(DWBuilderRef br, EagleTypeType *ty)
{
    SmallVector<Metadata *, 8> elems;

    EagleFunctionType *ft = (EagleFunctionType *)ty;
    // Add the result type.
    elems.push_back(DWEagleToDIType(br, ft->retType));
    
    for (int i = 0; i < ft->pct; i++)
        elems.push_back(DWEagleToDIType(br, ft->params[i]));
    
    return br->dib->createSubroutineType(br->unit, br->dib->getOrCreateTypeArray(elems));
}

DIType *DWEagleToDIType(DWBuilderRef br, EagleTypeType *ty)
{
    switch(ty->type)
    {
        case ETFunction:
            return DWFunctionType(br, ty);
        case ETInt32:
            return br->dib->createBasicType("int", 32, 32, dwarf::DW_ATE_signed);
        default:
            return NULL;
    }
}

