#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "mutex.h"

struct chan_item {
    uint32_t lap;
    void *data;
};

struct chan {
    // Unbuffered channels only: guarantees that at most
    // one writer and one reader have the right to access.
    struct mutex send_mtx;
    uint8_t pad1[CACHELINE_SIZE - sizeof(struct mutex)];
    struct mutex recv_mtx;
    uint8_t pad2[CACHELINE_SIZE - sizeof(struct mutex)];

    uint64_t head;
    uint8_t pad3[CACHELINE_SIZE - sizeof(uint64_t)];
    uint64_t tail;
    uint8_t pad4[CACHELINE_SIZE - sizeof(uint64_t)];

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

    bool closed;

    // Unbuffered channels only: the pointer used for data exchange.
    void **datap;

    // XXX problematic
    uint8_t pad5[CACHELINE_SIZE - 2 * sizeof(uint32_t)
                                - 2 * sizeof(size_t)
                                - sizeof(bool)
                                - sizeof(void **)];

    size_t cap;
    uint8_t pad6[CACHELINE_SIZE - sizeof(size_t)];

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
