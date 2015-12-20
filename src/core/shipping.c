#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "cpp/cpp.h"
#include "shipping.h"
#include "hashtable.h"

#define IN(x, chr) (hst_get(&x, (char *)chr, NULL, NULL))

extern hashtable global_args;
typedef LLVMPassManagerBuilderRef LPMB;

void shp_optimize(LLVMModuleRef module)
{
    LPMB passBuilder = LLVMPassManagerBuilderCreate();
    LLVMPassManagerRef pm = LLVMCreatePassManager();

    unsigned opt = 2;

    if(IN(global_args, "-O0"))
        opt = 0;
    else if(IN(global_args, "-O1"))
        opt = 1;
    else if(IN(global_args, "-O3"))
        opt = 3;

    LLVMPassManagerBuilderSetOptLevel(passBuilder, opt);
    LLVMPassManagerBuilderPopulateModulePassManager(passBuilder, pm);

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();

    LLVMTargetDataRef td = LLVMCreateTargetData("");
    LLVMAddTargetData(td, pm);

    LLVMRunPassManager(pm, module);

    LLVMPassManagerBuilderDispose(passBuilder);
}

void shp_produce_assembly(LLVMModuleRef module)
{
    EGLGenerateAssembly(module);
}

void shp_produce_binary()
{
    const char *outfile = "a.out";
    if(IN(global_args, "-o"))
        outfile = hst_get(&global_args, (char *)"-o", NULL, NULL);

    char command[500 + strlen(outfile)];

    const char *objectfile = "";
    const char *rcfile = "rc.o";
    if(IN(global_args, "--compile-rc"))
    {
        objectfile = "-c";
        rcfile = "";
    }

    sprintf(command, "cc %s /tmp/egl_out.s %s -o %s", objectfile, rcfile, outfile);
    
    system(command);
}

