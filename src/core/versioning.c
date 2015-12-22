#include "versioning.h"
#include <stdio.h>

void print_version_info()
{
    printf("Eagle Reference Compiler\n\nCopyright (C) 2015-2016, Sam Horlbeck Olsen\nVersion:\t"
        EGL_VERSION "\n");
    printf("Compiled:\t" __DATE__ " at " __TIME__ "\n");
}
