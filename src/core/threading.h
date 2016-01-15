#ifndef THREADING_H
#define THREADING_H

#include "llvm_headers.h"
#include "arraylist.h"

typedef struct {
    LLVMModuleRef module;
    LLVMContextRef context;
    char *filename;
} thr_work_bundle;

int thr_request_number();

void thr_init();
void thr_add_module_to_produce_assembly(LLVMModuleRef module);
void thr_await();

#endif
