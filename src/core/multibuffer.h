/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef MULTIBUFFER_H
#define MULTIBUFFER_H

#include <stdio.h>

typedef void (*mb_callback)(void *);

typedef struct multibuffer multibuffer;

multibuffer *mb_alloc();
void mb_add_file(multibuffer *buf, const char *filename);
void mb_add_str(multibuffer *buf, const char *c);
int mb_buffer(multibuffer *buf, char *dest, size_t max_size);
void mb_rewind(multibuffer *buf);
void mb_free(multibuffer *buf);
char *mb_get_first_str(multibuffer *buf);
void mb_print_all(multibuffer *buf);

#endif
