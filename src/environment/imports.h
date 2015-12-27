#ifndef IMPORTS_H
#define IMPORTS_H

#include "core/multibuffer.h"

char *imp_scan_file(const char *filename);
multibuffer *imp_generate_imports(const char *filename);

#endif
