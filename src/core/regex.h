/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef REGEX_H
#define REGEX_H

#define RGX_GLOBAL 1
#define RGX_IGNORE_CASE 2

struct Regex;
typedef struct Regex Regex;

typedef struct {
    int *indices;
    int ct;
    int alloced;
} RegexResultWrapper;

RegexResultWrapper rgx_wrapper_make();
void rgx_wrapper_append(RegexResultWrapper *w, int c);
int *rgx_wrapper_finalize(RegexResultWrapper *w);

Regex *rgx_compile(char *input);
void rgx_set_flags(Regex *regex, unsigned flags);
unsigned rgx_get_flags(Regex *regex);
int *rgx_collect_matches(Regex *regex, char *input);
int rgx_matches(Regex *regex, char *input);
int rgx_search(Regex *regex, char *input);
void rgx_free(Regex *regex);

#endif //REGEX_H
