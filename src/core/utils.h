#ifndef UTILS_H
#define UTILS_H

char *utl_gen_escaped_string(char *inp, int lineno);
void utl_register_memory(void *m);
void utl_free_registered();

#endif
