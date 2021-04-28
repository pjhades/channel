#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <assert.h>
#include <err.h>
#include "chan.h"

#ifdef __STDC_NO_ATOMICS__
#error "atomic library is unavailable"
#else
#include "atomic.h"
#endif

enum {
    MSG_MAX = 100000,
    THREAD_MAX = 1024,
};

struct thread_arg {
    size_t id;
    size_t lo;
    size_t hi;
};

#ifdef __STDC_NO_THREADS__
#include <pthread.h>
typedef pthread_t thread_id;
typedef void *(*thread_fn)(void *);
#define def_thread_fn(name)         void *name(void *arg)
#define thread_create(tid, fn, arg) pthread_create(tid, NULL, fn, arg)
#define thread_join(tid, ptr)       pthread_join(tid, ptr)
#else
#include <threads.h>
typedef thrd_t thread_id;
typedef int (*thread_fn)(void *);
#define def_thread_fn(name)         int name(void *arg)
#define thread_create(tid, fn, arg) thrd_create(tid, fn, arg)
#define thread_join(tid, ptr)       thrd_join(tid, ptr)
#endif

extern _Atomic size_t msg_count[MSG_MAX];

typedef void (*setup_fn)(void);
typedef void (*teardown_fn)(void);

void test_chan(const size_t repeat,
               const size_t total,
               const size_t n_readers, thread_fn reader_fn,
               const size_t n_writers, thread_fn writer_fn,
               setup_fn setup, teardown_fn teardown);
