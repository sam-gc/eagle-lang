/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef SHIPPING_H
#define SHIPPING_H

#include "llvm_headers.h"
#include "arraylist.h"

#define IN(x, chr) (hst_get(&x, (char *)chr, NULL, NULL))

typedef struct {
    unsigned verbose : 1;

    Arraylist source_files;
    Arraylist object_files;
    Arraylist extra_code;
    Arraylist work;
    Arraylist libs;
    Arraylist lib_paths;

    int widex;

    int threadct;
} ShippingCrate;

void shp_optimize(LLVMModuleRef module);
void shp_produce_assembly(LLVMModuleRef module, char *filename, char **outname);
void shp_produce_binary(char *filename, char *assemblyname, char **outname);
void shp_produce_executable(ShippingCrate *crate);

#endif
