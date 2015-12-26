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
