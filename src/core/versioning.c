/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "versioning.h"
#include "config.h"
#include <stdio.h>

void print_version_info()
{
    printf("Eagle Reference Compiler\n\nCopyright (C) 2015-2016, Sam Horlbeck Olsen\nVersion:\t"
        EGL_VERSION "\n");
    printf("Compiled:\t" __DATE__ " at " __TIME__ "\n");
}

void print_help_info(const char *progname)
{
    printf("Usage: %s [options] code-file(s)\nOptions:\n%s", progname, help_options);
}

