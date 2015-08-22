#include <llvm/IR/IRBuilder.h>
#include "cpp.h"

using namespace llvm;

LLVMValueRef EGLBuildMalloc(LLVMBuilderRef B, LLVMTypeRef Ty, LLVMValueRef Before, const char *Name)
{
    Type* ITy = Type::getInt32Ty(unwrap(B)->GetInsertBlock()->getContext());
    Constant* AllocSize = ConstantExpr::getSizeOf(unwrap(Ty));
    AllocSize = ConstantExpr::getTruncOrBitCast(AllocSize, ITy);
    Instruction* Malloc = CallInst::CreateMalloc(unwrap<Instruction>(Before),
                                                 ITy, unwrap(Ty), AllocSize,
                                                 nullptr, nullptr, "");
    //return wrap(unwrap(B)->Insert(Malloc, Twine(Name)));
    return wrap(Malloc);
}

