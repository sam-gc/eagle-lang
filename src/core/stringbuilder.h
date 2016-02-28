/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef STRINGBUILDER_H
#define STRINGBUILDER_H

#include <stdlib.h>

typedef struct {
    char *buffer;
    size_t len;
    size_t alloced;
} Strbuilder;

void sb_init(Strbuilder *builder);
void sb_append(Strbuilder *builder, const char *text);

#endif
