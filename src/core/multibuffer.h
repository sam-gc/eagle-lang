#ifndef MULTIBUFFER_H
#define MULTIBUFFER_H

#include <stdio.h>

typedef struct multibuffer multibuffer;

multibuffer *mb_alloc();
void mb_add_file(multibuffer *buf, const char *filename);
void mb_add_str(multibuffer *buf, const char *c);
int mb_buffer(multibuffer *buf, char *dest, size_t max_size);
void mb_rewind(multibuffer *buf);
void mb_free(multibuffer *buf);
char *mb_get_first_str(multibuffer *buf);

#endif
