#ifndef THREADING_H
#define THREADING_H

#include "llvm_headers.h"
#include "arraylist.h"
#include "shipping.h"

typedef struct {
    LLVMModuleRef module;
    LLVMContextRef context;
    char *filename;
    char *assemblyname;
} ThreadingBundle;

int thr_request_number();

ThreadingBundle *thr_create_bundle(LLVMModuleRef module, LLVMContextRef context, char *filename);
void thr_init();
void thr_teardown();
void thr_produce_machine_code(ShippingCrate *crate);
void thr_populate_pass_manager(LLVMPassManagerBuilderRef pbr, LLVMPassManagerRef pm);

char *thr_temp_object_file(char *filename);
char *thr_temp_assembly_file(char *filename);

#endif
