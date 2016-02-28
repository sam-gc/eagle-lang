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

struct regex;
typedef struct regex rgx_regex;

typedef struct {
    int *indices;
    int ct;
    int alloced;
} rgx_result_wrapper;

rgx_result_wrapper rgx_wrapper_make();
void rgx_wrapper_append(rgx_result_wrapper *w, int c);
int *rgx_wrapper_finalize(rgx_result_wrapper *w);

rgx_regex *rgx_compile(char *input);
void rgx_set_flags(rgx_regex *regex, unsigned flags);
unsigned rgx_get_flags(rgx_regex *regex);
int *rgx_collect_matches(rgx_regex *regex, char *input);
int rgx_matches(rgx_regex *regex, char *input);
int rgx_search(rgx_regex *regex, char *input);
void rgx_free(rgx_regex *regex);

#endif //REGEX_H
