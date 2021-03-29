#pragma once

#ifdef __STDC_NO_ATOMICS__
#error "no atomic library is available"
#else
#include <stdatomic.h>
#endif

#ifdef __STDC_NO_THREADS__
#include <pthread.h>
typedef pthread_t thread_id;
typedef void *(*thread_fn)(void *);
#define def_thread_fn(name) \
    void *name(void *arg)
#define thread_create(tid, fn, arg) \
    pthread_create(tid, NULL, fn, arg)
#define thread_join(tid, ptr) \
    pthread_join(tid, ptr)
#else
#include <threads.h>
typedef thrd_t thread_id;
typedef int (*thread_fn)(void *);
#define def_thread_fn(name) \
    int name(void *arg)
#define thread_create(tid, fn, arg) \
    thrd_create(tid, fn, arg)
#define thread_join(tid, ptr) \
    thrd_join(tid, ptr)
#endif
