#include <llvm/ADT/Triple.h>
#include <llvm/CodeGen/CommandFlags.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/PassSupport.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Support/TargetSelect.h>
#include "core/config.h"

#include <string>
#include <iostream>
#include "cpp.h"

#define unwrap llvm::unwrap

LLVMValueRef EGLBuildMalloc(LLVMBuilderRef B, LLVMTypeRef Ty, LLVMValueRef Before, const char *Name)
{
    llvm::Type* ITy = llvm::Type::getInt32Ty(unwrap(B)->GetInsertBlock()->getContext());
    llvm::Constant* AllocSize = llvm::ConstantExpr::getSizeOf(unwrap(Ty));
    AllocSize = llvm::ConstantExpr::getTruncOrBitCast(AllocSize, ITy);
    llvm::Instruction* Malloc = llvm::CallInst::CreateMalloc(unwrap<llvm::Instruction>(Before),
                                                 ITy, unwrap(Ty), AllocSize,
                                                 nullptr, nullptr, "");
    //return wrap(unwrap(B)->Insert(Malloc, Twine(Name)));
    return wrap(Malloc);
}

void EGLEraseFunction(LLVMValueRef func)
{
    unwrap<llvm::Function>(func)->eraseFromParent();
}
