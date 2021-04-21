#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "mutex.h"

struct unbuf_chan {
    _Atomic bool closed;

    // pointer used for data exchange.
    _Atomic(void **) datap;

    // guarantees that at most one writer and
    // one reader have the right to access.
    struct mutex send_mtx;
    struct mutex recv_mtx;

    // These futexes start from 1 (CHAN_NOT_READY).
    // They are incremented to indicate that a thread is waiting.
    // They are decremented to indicate that data exchange is done.
    _Atomic uint32_t send_ftx;
    _Atomic uint32_t recv_ftx;
};

struct chan_item {
    _Atomic uint32_t lap;
    void *data;
};

struct buf_chan {
    _Atomic bool closed;

    // These futexes represent credits for a reader or
    // write to retry receiving or sending.
    _Atomic uint32_t send_ftx;
    _Atomic uint32_t recv_ftx;

    // Number of waiting threads on the futexes.
    _Atomic size_t send_waiters;
    _Atomic size_t recv_waiters;

    // Ring buffer
    _Atomic uint64_t head;
    _Atomic uint64_t tail;
    size_t cap;
    struct chan_item *ring;
};

typedef void *(*chan_alloc_fn)(size_t);
typedef void (*chan_free_fn)(void *);

struct chan *chan_make(size_t cap, chan_alloc_fn alloc);
struct chan chan_make_unbuf(void);
void chan_close(struct chan *ch);
int chan_send(struct chan *ch, void *data);
int chan_recv(struct chan *ch, void **data);
int chan_trysend(struct chan *ch, void *data);
int chan_tryrecv(struct chan *ch, void **data);
