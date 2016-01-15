#ifndef SHIPPING_H
#define SHIPPING_H

#include "llvm_headers.h"
#include "arraylist.h"

#define IN(x, chr) (hst_get(&x, (char *)chr, NULL, NULL))

typedef struct {
    arraylist source_files;
    arraylist object_files;
    arraylist extra_code;
    char *current_file;
    char *current_temp_assembly;
} ShippingCrate;

void shp_setup();
void shp_teardown();
void shp_optimize(LLVMModuleRef module);
void shp_produce_assembly(LLVMModuleRef module, ShippingCrate *crate);
void shp_produce_binary(ShippingCrate *crate);
void shp_produce_executable(ShippingCrate *crate);

#endif
