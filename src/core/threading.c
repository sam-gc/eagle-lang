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

typedef pthread_mutex_t th_mutex;
typedef pthread_t th_thread;
#define th_init_mutex(mut) pthread_mutex_init(&(mut), NULL)
#define th_lock(mut) pthread_mutex_lock(&(mut))
#define th_unlock(mut) pthread_mutex_unlock(&(mut))
#define th_destroy_mutex(mut) pthread_mutex_destroy(&(mut))
#define th_split(thread, func, data) pthread_create(&(thread), NULL, (func), (data))
#define th_join(thread) pthread_join((thread), NULL)
#define threadct() 4

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

static th_mutex number_lock;
static th_mutex name_lock;
static th_mutex work_lock;
static th_mutex obj_lock;

static mempool unlink_pool;

typedef struct {
    ShippingCrate *crate;
    int thread_num;
} ProcData;

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

ThreadingBundle *thr_get_next_work(ShippingCrate *crate)
{
    th_lock(work_lock);
    ThreadingBundle *out = NULL;

    if(crate->widex < crate->work.count)
        out = crate->work.items[crate->widex++];

    th_unlock(work_lock);

    return out;
}

void *thr_work_proc(void *data)
{
    ThreadingBundle *bundle;
    ProcData *pd = data;
    ShippingCrate *crate = pd->crate;

    int ct = 0;
    while((bundle = thr_get_next_work(crate)))
    {
        shp_optimize(bundle->module);
        char *out = NULL;
        shp_produce_assembly(bundle->module, bundle->filename, &out);
        char *object = NULL;
        shp_produce_binary(bundle->filename, out, &object);

        th_lock(obj_lock);
        arr_append(&crate->object_files, object);
        th_unlock(obj_lock);
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
    for(int i = 0; i < thrct; i++)
    {
        ProcData *pd = malloc(sizeof(ProcData));
        pd->thread_num = i + 1;
        pd->crate = crate;
        th_split(threads[i], thr_work_proc, pd);
    }

    for(int i = 0; i < thrct; i++)
        th_join(threads[i]);

    // for(int i = 0; i < crate->work.count; i++)
    // {
    //     ThreadingBundle *bundle = crate->work.items[i];
    //     shp_optimize(bundle->module);
    //     char *out = NULL;
    //     shp_produce_assembly(bundle->module, bundle->filename, &out);
    //     char *object = NULL;
    //     shp_produce_binary(bundle->filename, out, &object);

    //     arr_append(&crate->object_files, object);
    // }
}

char *thr_temp_object_file(char *filename)
{
    th_lock(name_lock);
    char *dup = strdup(filename);
    char *base = basename(dup);
    strip_ext(base);

    char *name = malloc(100);
    sprintf(name, "/tmp/egl%d_%s.o", thr_request_number(), base);
    free(dup);

    pool_add(&unlink_pool, name);
    th_unlock(name_lock);
    return name;
}

char *thr_temp_assembly_file(char *filename)
{
    th_lock(name_lock);
    char *dup = strdup(filename);
    char *base = basename(dup);
    strip_ext(base);

    char *name = malloc(100);
    sprintf(name, "/tmp/egl%d_%s.s", thr_request_number(), base);
    free(dup);

    pool_add(&unlink_pool, name);
    th_unlock(name_lock);
    return name;
}
