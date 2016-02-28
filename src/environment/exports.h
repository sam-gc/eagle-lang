/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef EXPORTS_H
#define EXPORTS_H

typedef struct export_control export_control;

export_control *ec_alloc();
void ec_add_str(export_control *ec, const char *str);
void ec_add_wcard(export_control *ec, const char *str);
int ec_allow(export_control *ec, const char *str);
void ec_free(export_control *ec);

#endif
