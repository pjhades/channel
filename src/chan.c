#include <stdint.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include "futex.h"
#include "chan.h"

enum {
    CHAN_READY = 0,
    CHAN_NOT_READY = 1,
    CHAN_WAITING = 2,
    CHAN_CLOSED = 3,
};

static void chan_init(struct chan *ch, size_t cap) {
    ch->closed = false;
    ch->datap = NULL;

    mutex_init(&ch->send_mtx);
    mutex_init(&ch->recv_mtx);

    if (cap == 0) {
        ch->send_ftx = CHAN_NOT_READY;
        ch->recv_ftx = CHAN_NOT_READY;
    }
    else {
        ch->send_ftx = 0;
        ch->recv_ftx = 0;
    }
    ch->send_waiters = 0;
    ch->recv_waiters = 0;
    ch->cap = cap;
    ch->head = (uint64_t)1 << 32;
    ch->tail = 0;
    if (ch->cap > 0)
        memset(ch->ring, 0, cap * sizeof(struct chan_item));
}

struct chan *chan_make(size_t cap, chan_alloc_fn alloc) {
    struct chan *ch;
    if (!alloc || !(ch = alloc(sizeof(*ch) + cap * sizeof(struct chan_item))))
        return NULL;
    chan_init(ch, cap);
    return ch;
}

struct chan chan_make_unbuf(void) {
    struct chan ch;
    chan_init(&ch, 0);
    return ch;
}

static int chan_trysend_buf(struct chan *ch, void *data) {
    if (atomic_load_explicit(&ch->closed, memory_order_relaxed)) {
        errno = EPIPE;
        return -1;
    }

    uint64_t tail, new_tail;
    uint32_t pos, lap;
    struct chan_item *item;

    do {
        tail = atomic_load_explicit(&ch->tail, memory_order_acquire);
        pos = (uint32_t)tail;
        lap = (uint32_t)(tail >> 32);
        item = ch->ring + pos;

        if (atomic_load_explicit(&item->lap, memory_order_acquire) != lap) {
            errno = EAGAIN;
            return -1;
        }

        if (pos + 1 == ch->cap)
            new_tail = (uint64_t)(lap + 2) << 32;
        else
            new_tail = tail + 1;
    } while (!atomic_compare_exchange_weak_explicit(
                 &ch->tail, &tail, new_tail,
                 memory_order_acq_rel,
                 memory_order_acquire));

    item->data = data;
    atomic_fetch_add_explicit(&item->lap, 1, memory_order_release);

    return 0;
}

static int chan_send_buf(struct chan *ch, void *data) {
    while (chan_trysend_buf(ch, data) == -1) {
        if (errno != EAGAIN)
            return -1;

        uint32_t v = 1;
        while (!atomic_compare_exchange_weak_explicit(
                   &ch->send_ftx, &v, v - 1,
                   memory_order_acq_rel,
                   memory_order_acquire)) {
            if (v == 0) {
                atomic_fetch_add_explicit(&ch->send_waiters, 1, memory_order_acq_rel);
                futex_wait(&ch->send_ftx, 0);
                atomic_fetch_sub_explicit(&ch->send_waiters, 1, memory_order_acq_rel);
                v = 1;
            }
        }
    }

    atomic_fetch_add_explicit(&ch->recv_ftx, 1, memory_order_acq_rel);

    if (atomic_load_explicit(&ch->recv_waiters, memory_order_relaxed) > 0)
        futex_wake(&ch->recv_ftx, 1);

    return 0;
}

static int chan_tryrecv_buf(struct chan *ch, void **data) {
    if (atomic_load_explicit(&ch->closed, memory_order_relaxed)) {
        errno = EPIPE;
        return -1;
    }

    uint64_t head, new_head;
    uint32_t pos, lap;
    struct chan_item *item;

    do {
        head = atomic_load_explicit(&ch->head, memory_order_acquire);
        pos = (uint32_t)head;
        lap = (uint32_t)(head >> 32);
        item = ch->ring + pos;

        if (atomic_load_explicit(&item->lap, memory_order_acquire) != lap) {
            errno = EAGAIN;
            return -1;
        }

        if (pos + 1 == ch->cap)
            new_head = (uint64_t)(lap + 2) << 32;
        else
            new_head = head + 1;
    } while (!atomic_compare_exchange_weak_explicit(
                 &ch->head, &head, new_head,
                 memory_order_acq_rel,
                 memory_order_acquire));

    *data = item->data;
    atomic_fetch_add_explicit(&item->lap, 1, memory_order_release);

    return 0;
}

static int chan_recv_buf(struct chan *ch, void **data) {
    while (chan_tryrecv_buf(ch, data) == -1) {
        if (errno != EAGAIN)
            return -1;

        uint32_t v = 1;
        while (!atomic_compare_exchange_weak_explicit(
                   &ch->recv_ftx, &v, v - 1,
                   memory_order_acq_rel,
                   memory_order_acquire)) {
            if (v == 0) {
                atomic_fetch_add_explicit(&ch->recv_waiters, 1, memory_order_acq_rel);
                futex_wait(&ch->recv_ftx, 0);
                atomic_fetch_sub_explicit(&ch->recv_waiters, 1, memory_order_acq_rel);
                v = 1;
            }
        }
    }

    atomic_fetch_add_explicit(&ch->send_ftx, 1, memory_order_acq_rel);

    if (atomic_load_explicit(&ch->send_waiters, memory_order_relaxed) > 0)
        futex_wake(&ch->send_ftx, 1);

    return 0;
}

static int chan_send_unbuf(struct chan *ch, void *data) {
    if (atomic_load_explicit(&ch->closed, memory_order_relaxed)) {
        errno = EPIPE;
        return -1;
    }

    mutex_lock(&ch->send_mtx);

    void **ptr = NULL;
    if (!atomic_compare_exchange_strong_explicit(
            &ch->datap, &ptr, &data,
            memory_order_acq_rel,
            memory_order_acquire)) {
        *ptr = data;
        atomic_store_explicit(&ch->datap, NULL, memory_order_release);

        if (atomic_fetch_sub_explicit(
                &ch->recv_ftx, 1,
                memory_order_acquire) == CHAN_WAITING)
            futex_wake(&ch->recv_ftx, 1);
    }
    else {
        if (atomic_fetch_add_explicit(
                &ch->send_ftx, 1,
                memory_order_acquire) == CHAN_NOT_READY) {
            do {
                futex_wait(&ch->send_ftx, CHAN_WAITING);
            } while (atomic_load_explicit(
                         &ch->send_ftx,
                         memory_order_acquire) == CHAN_WAITING);

            if (atomic_load_explicit(&ch->closed, memory_order_relaxed)) {
                errno = EPIPE;
                return -1;
            }
        }
    }

    mutex_unlock(&ch->send_mtx);
    return 0;
}

static int chan_trysend_unbuf(struct chan *ch, void *data) {
    if (atomic_load_explicit(&ch->closed, memory_order_relaxed)) {
        errno = EPIPE;
        return -1;
    }

    if (mutex_trylock(&ch->send_mtx) == -1) {
        errno = EAGAIN;
        return -1;
    }

    if (atomic_load_explicit(
            &ch->recv_ftx,
            memory_order_acquire) != CHAN_WAITING) {
        mutex_unlock(&ch->send_mtx);
        errno = EAGAIN;
        return -1;
    }

    *ch->datap = data;
    ch->datap = NULL;

    atomic_fetch_sub_explicit(&ch->recv_ftx, 1, memory_order_release);
    futex_wake(&ch->recv_ftx, 1);

    mutex_unlock(&ch->send_mtx);
    return 0;
}

static int chan_recv_unbuf(struct chan *ch, void **data) {
    if (!data) {
        errno = EINVAL;
        return -1;
    }

    if (atomic_load_explicit(&ch->closed, memory_order_relaxed)) {
        errno = EPIPE;
        return -1;
    }

    mutex_lock(&ch->recv_mtx);

    void **ptr = NULL;
    if (!atomic_compare_exchange_strong_explicit(
            &ch->datap, &ptr, data,
            memory_order_acq_rel,
            memory_order_acquire)) {
        *data = *ptr;
        atomic_store_explicit(&ch->datap, NULL, memory_order_release);

        if (atomic_fetch_sub_explicit(
                &ch->send_ftx, 1,
                memory_order_acquire) == CHAN_WAITING)
            futex_wake(&ch->send_ftx, 1);
    }
    else {
        if (atomic_fetch_add_explicit(
                &ch->recv_ftx, 1,
                memory_order_acquire) == CHAN_NOT_READY) {
            do {
                futex_wait(&ch->recv_ftx, CHAN_WAITING);
            } while (atomic_load_explicit(
                         &ch->recv_ftx,
                         memory_order_acquire) == CHAN_WAITING);

            if (atomic_load_explicit(&ch->closed, memory_order_relaxed)) {
                errno = EPIPE;
                return -1;
            }
        }
    }

    mutex_unlock(&ch->recv_mtx);
    return 0;
}

static int chan_tryrecv_unbuf(struct chan *ch, void **data) {
    if (!data) {
        errno = EINVAL;
        return -1;
    }

    if (atomic_load_explicit(&ch->closed, memory_order_relaxed)) {
        errno = EPIPE;
        return -1;
    }

    if (mutex_trylock(&ch->recv_mtx) == -1) {
        errno = EAGAIN;
        return -1;
    }

    if (atomic_load_explicit(
            &ch->send_ftx,
            memory_order_acquire) != CHAN_WAITING) {
        mutex_unlock(&ch->recv_mtx);
        errno = EAGAIN;
        return -1;
    }

    *data = *ch->datap;
    ch->datap = NULL;

    atomic_fetch_sub_explicit(&ch->send_ftx, 1, memory_order_release);
    futex_wake(&ch->send_ftx, 1);

    mutex_unlock(&ch->recv_mtx);
    return 0;
}

void chan_close(struct chan *ch) {
    ch->closed = true;
    if (ch->cap == 0) {
        atomic_store(&ch->recv_ftx, CHAN_CLOSED);
        atomic_store(&ch->send_ftx, CHAN_CLOSED);
    }
    futex_wake(&ch->recv_ftx, INT_MAX);
    futex_wake(&ch->send_ftx, INT_MAX);
}

int chan_send(struct chan *ch, void *data) {
    return (ch->cap == 0)
           ? chan_send_unbuf(ch, data)
           : chan_send_buf(ch, data);
}

int chan_recv(struct chan *ch, void **data) {
    return (ch->cap == 0)
           ? chan_recv_unbuf(ch, data)
           : chan_recv_buf(ch, data);
}

int chan_trysend(struct chan *ch, void *data) {
    return (ch->cap == 0)
           ? chan_trysend_unbuf(ch, data)
           : chan_trysend_buf(ch, data);
}

int chan_tryrecv(struct chan *ch, void **data) {
    return (ch->cap == 0)
           ? chan_tryrecv_unbuf(ch, data)
           : chan_tryrecv_buf(ch, data);
}
