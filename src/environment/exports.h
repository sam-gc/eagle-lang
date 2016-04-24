/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef EXPORTS_H
#define EXPORTS_H

typedef struct ExportControl ExportControl;

ExportControl *ec_alloc();
void ec_add_str(ExportControl *ec, const char *str, int token);
void ec_add_wcard(ExportControl *ec, const char *str, int token);
int ec_allow(ExportControl *ec, const char *str, int token);
void ec_free(ExportControl *ec);

#endif
