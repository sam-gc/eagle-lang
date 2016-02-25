#ifndef SHIPPING_H
#define SHIPPING_H

#include "llvm_headers.h"
#include "arraylist.h"

#define IN(x, chr) (hst_get(&x, (char *)chr, NULL, NULL))

typedef struct {
    arraylist source_files;
    arraylist object_files;
    arraylist extra_code;
    arraylist work;
    arraylist libs;

    int widex;
} ShippingCrate;

void shp_optimize(LLVMModuleRef module);
void shp_produce_assembly(LLVMModuleRef module, char *filename, char **outname);
void shp_produce_binary(char *filename, char *assemblyname, char **outname);
void shp_produce_executable(ShippingCrate *crate);

#endif
