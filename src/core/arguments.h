/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ARGUMENTS_H
#define ARGUMENTS_H

#include "hashtable.h"
#include "shipping.h"

void args_setup(ShippingCrate *crate);
void args_run(const char *argv[]);
void args_teardown();

#endif

