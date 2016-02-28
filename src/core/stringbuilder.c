/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <string.h>
#include "stringbuilder.h"

#define GROWTH_SIZE 250

void sb_init(Strbuilder *builder)
{
    builder->alloced = GROWTH_SIZE;
    builder->buffer = malloc(GROWTH_SIZE + 1);
    builder->buffer[0] = 0;
    builder->len = 0;
}

void sb_append(Strbuilder *builder, const char *text)
{
    size_t tlen = strlen(text);
    if(tlen + builder->len >= builder->alloced)
    {
        builder->buffer = realloc(builder->buffer, builder->alloced + tlen + GROWTH_SIZE + 1);
        builder->alloced += GROWTH_SIZE + tlen;
    }

    memcpy(builder->buffer + builder->len, text, tlen);
    builder->len += tlen;
    builder->buffer[builder->len] = 0;
}
