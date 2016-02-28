/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef UTILS_H
#define UTILS_H

#include "llvm_headers.h"

char *utl_gen_escaped_string(char *inp, int lineno);
void utl_register_memory(void *m);
void utl_free_registered();
void utl_set_current_context(LLVMContextRef ctx);
LLVMContextRef utl_get_current_context();

#endif
