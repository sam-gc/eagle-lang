#include <pthread.h>
#include "threading.h"
#include "config.h"

#ifdef HAS_PTHREAD

typedef pthread_mutex_t th_mutex;
#define th_init_mutex(mut) pthread_mutex_init(&(mut), NULL)
#define th_lock(mut) pthread_mutex_lock(&(mut))
#define th_unlock(mut) pthread_mutex_unlock(&(mut))

#else

typedef int th_mutex;
#define th_init_mutex(mut)
#define th_lock(mut)
#define th_unlock(mut)

#endif

static th_mutex number_lock;

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
}
