#ifndef SHIPPING_H
#define SHIPPING_H

#include "llvm_headers.h"

#define IN(x, chr) (hst_get(&x, (char *)chr, NULL, NULL))

void shp_optimize(LLVMModuleRef module);
void shp_produce_assembly(LLVMModuleRef module);
void shp_produce_binary();

#endif
