/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef HASHTABLE_H
#define HASHTABLE_H

typedef long (*hst_hash_function)(void *key, void *data);
typedef int  (*hst_equa_function)(void *key, void *data);
typedef void (*hst_each_function)(void *key, void *val, void *data);

struct HSTNode_s;
typedef struct HSTNode_s HSTNode;

typedef struct hashtable_s {
    int count;
    int size;
    char duplicate_keys;
    HSTNode **buckets;
} Hashtable;

Hashtable hst_create();
void hst_put(Hashtable *ht, void *key, void *val, hst_hash_function hashfunc, hst_equa_function equfunc);
void *hst_get(Hashtable *ht, void *key, hst_hash_function hashfunc, hst_equa_function equfunc);
int hst_contains_key(Hashtable *ht, void *key, hst_hash_function hashfunc, hst_equa_function equfunc);
int hst_contains_value(Hashtable *ht, void *val, hst_equa_function equfunc);
void hst_add_all_from(Hashtable *ht, Hashtable *ot, hst_hash_function hashfunc, hst_equa_function equfunc);
void *hst_remove_key(Hashtable *ht, void *key, hst_hash_function hashfunc, hst_equa_function equfunc);
void hst_remove_val(Hashtable *ht, void *val, hst_equa_function equfunc);
void hst_free(Hashtable *ht);
void hst_for_each(Hashtable *ht, hst_each_function func, void *data);
char *hst_retrieve_duped_key(Hashtable *ht, char *key);

long hst_djb2(void *val, void *data);

#endif
