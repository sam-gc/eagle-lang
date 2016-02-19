/* Lanky -- Scripting Language and Virtual Machine
 * Copyright (C) 2014  Sam Olsen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef HASHTABLE_H
#define HASHTABLE_H

typedef long (*hst_hash_function)(void *key, void *data);
typedef int  (*hst_equa_function)(void *key, void *data);
typedef void (*hst_each_function)(void *key, void *val, void *data);

typedef struct hst_node_s {
    long hash;
    void *key;
    void *val;
    struct hst_node_s *next;
} hst_node;

typedef struct hashtable_s {
    int count;
    int size;
    char duplicate_keys;
    hst_node **buckets;
} hashtable;

hashtable hst_create();
void hst_put(hashtable *ht, void *key, void *val, hst_hash_function hashfunc, hst_equa_function equfunc); 
void *hst_get(hashtable *ht, void *key, hst_hash_function hashfunc, hst_equa_function equfunc);
int hst_contains_key(hashtable *ht, void *key, hst_hash_function hashfunc, hst_equa_function equfunc);
int hst_contains_value(hashtable *ht, void *val, hst_equa_function equfunc);
void hst_add_all_from(hashtable *ht, hashtable *ot, hst_hash_function hashfunc, hst_equa_function equfunc);
void *hst_remove_key(hashtable *ht, void *key, hst_hash_function hashfunc, hst_equa_function equfunc);
void hst_remove_val(hashtable *ht, void *val, hst_equa_function equfunc);
void hst_free(hashtable *ht);
void hst_for_each(hashtable *ht, hst_each_function func, void *data);

long hst_djb2(void *val, void *data);

#endif
