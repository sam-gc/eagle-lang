#ifndef UTILS_H
#define UTILS_H

#include "llvm_headers.h"

char *utl_gen_escaped_string(char *inp, int lineno);
void utl_register_memory(void *m);
void utl_free_registered();
void utl_set_current_context(LLVMContextRef ctx);
LLVMContextRef utl_get_current_context();

#endif
