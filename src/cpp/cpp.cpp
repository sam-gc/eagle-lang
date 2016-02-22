#include <llvm/IR/IRBuilder.h>
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
