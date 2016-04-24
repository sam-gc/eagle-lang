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

typedef struct Multibuffer Multibuffer;

Multibuffer *mb_alloc();
void mb_add_file(Multibuffer *buf, const char *filename);
void mb_add_str(Multibuffer *buf, const char *c);
int mb_buffer(Multibuffer *buf, char *dest, size_t max_size);
void mb_rewind(Multibuffer *buf);
void mb_free(Multibuffer *buf);
char *mb_get_first_str(Multibuffer *buf);
void mb_print_all(Multibuffer *buf);

#endif
