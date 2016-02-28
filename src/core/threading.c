/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <libgen.h>
#include <unistd.h>
#include "threading.h"
#include "config.h"
#include "mempool.h"
#include "hashtable.h"

extern hashtable global_args;

#define IN(x, chr) (hst_get(&x, (char *)chr, NULL, NULL))

#ifdef HAS_PTHREAD

#define THREADING

typedef pthread_mutex_t th_mutex;
typedef pthread_t th_thread;
#define th_init_mutex(mut) pthread_mutex_init(&(mut), NULL)
#define th_lock(mut) pthread_mutex_lock(&(mut))
#define th_unlock(mut) pthread_mutex_unlock(&(mut))
#define th_destroy_mutex(mut) pthread_mutex_destroy(&(mut))
#define th_split(thread, func, data) pthread_create(&(thread), NULL, (func), (data))
#define th_join(thread) pthread_join((thread), NULL)
#define threadct() thr_pthread_sys_count()

#else

typedef int th_mutex;
typedef int th_thread;
#define th_init_mutex(mut)
#define th_lock(mut)
#define th_unlock(mut)
#define th_destroy_mutex(mut)
#define threadct() 1
#define th_split(thread, func, data) (func)(data)
#define th_join(thread)

#endif

#ifdef THREADING
static th_mutex number_lock;
static th_mutex name_lock;
static th_mutex work_lock;
static th_mutex obj_lock;
static th_mutex llvm_lock;
#endif

static mempool unlink_pool;

typedef struct {
    ShippingCrate *crate;
    int thread_num;
    char **outputfiles;
} ProcData;

#ifdef HAS_PTHREAD

int thr_pthread_sys_count()
{
    if(!LLVMIsMultithreaded())
        return 1;

    return 4;
}

#endif

void strip_ext(char *base)
{
    int i;
    int len = strlen(base);
    for(i = len - 1; i >= 0; i--)
        if(base[i] == '.')
        {
            base[i] = 0;
            break;
        }
}

int thr_request_number()
{
    static int counter = 0;

    th_lock(number_lock);
    int num = counter++;
    th_unlock(number_lock);

    return num;
}

void thr_init()
{
    th_init_mutex(number_lock);
    th_init_mutex(name_lock);
    th_init_mutex(work_lock);
    th_init_mutex(obj_lock);
    th_init_mutex(llvm_lock);

    unlink_pool = pool_create();
    unlink_pool.free_func = (void (*)(void *))unlink;
}

void thr_teardown()
{
    pool_drain(&unlink_pool);

    th_destroy_mutex(number_lock);
    th_destroy_mutex(name_lock);
    th_destroy_mutex(work_lock);
    th_destroy_mutex(obj_lock);
    th_destroy_mutex(llvm_lock);
}

ThreadingBundle *thr_create_bundle(LLVMModuleRef module, LLVMContextRef context, char *filename)
{
    ThreadingBundle *bundle = malloc(sizeof(ThreadingBundle));
    bundle->filename  = filename;
    bundle->context = context;
    bundle->module = module;
    bundle->assemblyname = NULL;

    return bundle;
}

void thr_populate_pass_manager(LLVMPassManagerBuilderRef pbr, LLVMPassManagerRef pm)
{
    th_lock(llvm_lock);
    LLVMPassManagerBuilderPopulateModulePassManager(pbr, pm);
    th_unlock(llvm_lock);
}

ThreadingBundle *thr_get_next_work(ShippingCrate *crate, int *idx)
{
    th_lock(work_lock);
    ThreadingBundle *out = NULL;

    if(crate->widex < crate->work.count)
        out = crate->work.items[crate->widex++];

    *idx = crate->widex - 1;

    th_unlock(work_lock);

    return out;
}

void *thr_work_proc(void *data)
{
    ThreadingBundle *bundle;
    ProcData *pd = data;
    ShippingCrate *crate = pd->crate;

    int idx;
    int ct = 0;
    while((bundle = thr_get_next_work(crate, &idx)))
    {
        shp_optimize(bundle->module);
        char *out = NULL;
        shp_produce_assembly(bundle->module, bundle->filename, &out);
        char *object = NULL;
        shp_produce_binary(bundle->filename, out, &object);

        pd->outputfiles[idx] = object;
        ct += 1;
    }

    if(IN(global_args, "--verbose"))
        printf("Thread (%d) complete; compiled %d modules\n", pd->thread_num, ct);

    free(pd);
    return NULL;
}

void thr_produce_machine_code(ShippingCrate *crate)
{
    crate->widex = 0;

    int thrct = IN(global_args, "--threads") ? atoi(hst_get(&global_args, (char *)"--threads", NULL, NULL)) : threadct();

    if(IN(global_args, "--verbose"))
        printf("Generating machine code (%d threads)\n", thrct);

    th_thread threads[thrct];
    char *outputfiles[crate->work.count];

    for(int i = 0; i < thrct; i++)
    {
        ProcData *pd = malloc(sizeof(ProcData));
        pd->thread_num = i + 1;
        pd->crate = crate;
        pd->outputfiles = outputfiles;
        th_split(threads[i], thr_work_proc, pd);
    }

    for(int i = 0; i < thrct; i++)
        th_join(threads[i]);

    for(int i = 0; i < crate->work.count; i++)
        arr_append(&crate->object_files, outputfiles[i]);
}

char *thr_temp_object_file(char *filename)
{
    char *dup = strdup(filename);
    char *base = basename(dup);
    strip_ext(base);

    char *name = malloc(100);
    sprintf(name, "/tmp/egl%d_%s.o", thr_request_number(), base);
    free(dup);

    th_lock(name_lock);
    pool_add(&unlink_pool, name);
    th_unlock(name_lock);
    return name;
}

char *thr_temp_assembly_file(char *filename)
{
    char *dup = strdup(filename);
    char *base = basename(dup);
    strip_ext(base);

    char *name = malloc(100);
    sprintf(name, "/tmp/egl%d_%s.s", thr_request_number(), base);
    free(dup);

    th_lock(name_lock);
    pool_add(&unlink_pool, name);
    th_unlock(name_lock);
    return name;
}
