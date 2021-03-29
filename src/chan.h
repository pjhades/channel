#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "mutex.h"

struct chan_item {
    uint32_t lap;
    void *data;
};

struct chan {
    bool closed;

    // Unbuffered channels only: the pointer used for data exchange.
    void **datap;

    // Unbuffered channels only: guarantees that at most
    // one writer and one reader have the right to access.
    struct mutex send_mtx;
    struct mutex recv_mtx;

    // For unbuffered channels, these futexes start from 1 (CHAN_NOT_READY).
    // They are incremented to indicate that a thread is waiting.
    // They are decremented to indicate that data exchange is done.
    //
    // For buffered channels, these futexes represent credits for
    // a reader or write to retry receiving or sending.
    uint32_t send_ftx;
    uint32_t recv_ftx;

    // Buffered channels only: number of waiting threads on the futexes.
    size_t send_waiters;
    size_t recv_waiters;

    // Ring buffer
    size_t cap;
    uint64_t head;
    uint64_t tail;
    struct chan_item ring[0];
};

typedef void *(*chan_alloc_fn)(size_t);

struct chan *chan_make(size_t cap, chan_alloc_fn alloc);
struct chan chan_make_unbuf(void);
void chan_close(struct chan *ch);
int chan_send(struct chan *ch, void *data);
int chan_recv(struct chan *ch, void **data);
int chan_trysend(struct chan *ch, void *data);
int chan_tryrecv(struct chan *ch, void **data);
