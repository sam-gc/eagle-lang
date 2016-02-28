/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef AC_ENUM_H
#define AC_ENUM_H

void ac_make_enum_definitions(AST *ast, CompilerBundle *cb);
void ac_compile_enum_decleration(AST *ast, CompilerBundle *cb);

#endif
