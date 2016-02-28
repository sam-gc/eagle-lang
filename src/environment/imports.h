/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef IMPORTS_H
#define IMPORTS_H

#include "core/multibuffer.h"

char *imp_scan_file(const char *filename);
multibuffer *imp_generate_imports(const char *filename);

#endif
