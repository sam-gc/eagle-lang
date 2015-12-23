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

#ifdef llvm_OLD
#include <llvm/Support/FormattedStream.h>
#endif

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

// Code created by examining llc.cpp source
void EGLGenerateAssembly(LLVMModuleRef module)
{
    auto m = unwrap(module);
    llvm::sys::fs::OpenFlags flags = llvm::sys::fs::F_None | llvm::sys::fs::F_Text;

    InitializeAllTargets();
    InitializeAllAsmPrinters();
    InitializeAllTargetMCs();
    InitializeAllAsmParsers();

    std::error_code err;
    llvm::tool_output_file file(llvm::StringRef("/tmp/egl_out.s"), err, flags);

    llvm::legacy::PassManager pm;

    llvm::Triple trp;

#ifdef llvm_OLD
    trp.setTriple(LLVMGetDefaultTargetTriple());
#else
    trp.setTriple(llvm::sys::getDefaultTargetTriple());
#endif

    std::string error;
    const llvm::Target *targ = llvm::TargetRegistry::lookupTarget("x86-64", trp, error); 

    auto tm = targ->createTargetMachine(trp.getTriple(), "", "", llvm::TargetOptions());

#ifdef llvm_OLD
    auto stream = new llvm::formatted_raw_ostream(file.os());
    tm->addPassesToEmitFile(pm, *stream, llvm::TargetMachine::CodeGenFileType::CGFT_AssemblyFile);
#else
    tm->addPassesToEmitFile(pm, file.os(), llvm::TargetMachine::CodeGenFileType::CGFT_AssemblyFile);
#endif

    pm.run(*m);

    file.keep();

#ifdef llvm_OLD
    delete stream;
#endif
}

