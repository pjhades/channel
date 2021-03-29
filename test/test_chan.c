#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <x86intrin.h>
#include <errno.h>
#include <assert.h>
#include <err.h>

#include "chan.h"
#include "util.h"

enum {
    MSG_MAX = 100000,
    THREAD_MAX = 1024,
};

struct thread_arg {
    size_t id;
    size_t from;
    size_t to;
    struct chan *ch;
};

thread_id reader_tids[THREAD_MAX];
thread_id writer_tids[THREAD_MAX];
struct thread_arg reader_args[THREAD_MAX];
struct thread_arg writer_args[THREAD_MAX];
size_t msg_total, msg_count[MSG_MAX];

def_thread_fn(writer) {
    struct thread_arg *a = arg;

    for (size_t i = a->from; i < a->to; i++) {
        if (chan_send(a->ch, (void *)i) == -1)
            break;
    }
    return 0;
}

def_thread_fn(writer_nonblocking) {
    struct thread_arg *a = arg;

    for (size_t i = a->from; i < a->to; i++) {
        while (chan_trysend(a->ch, (void *)i) == -1) {
            if (errno != EAGAIN)
                return 0;
        }
    }
    return 0;
}

def_thread_fn(reader) {
    struct thread_arg *a = arg;
    size_t msg, received = 0, expect = a->to - a->from;

    while (received < expect) {
        if (chan_recv(a->ch, (void **)&msg) == -1)
            break;
        atomic_fetch_add_explicit(&msg_count[msg], 1, memory_order_relaxed);
        ++received;
    }
    return 0;
}

def_thread_fn(reader_nonblocking) {
    struct thread_arg *a = arg;
    size_t msg, received = 0, expect = a->to - a->from;

    while (received < expect) {
        while (chan_tryrecv(a->ch, (void **)&msg) == -1) {
            if (errno != EAGAIN)
                return 0;
        }
        atomic_fetch_add_explicit(&msg_count[msg], 1, memory_order_relaxed);
        ++received;
    }
    return 0;
}

void create_threads(const size_t n,
                    thread_fn fn,
                    struct thread_arg *args,
                    thread_id *tids,
                    struct chan *ch)
{
    size_t each = msg_total / n;
    size_t left = msg_total % n;
    size_t from = 0;

    for (size_t i = 0; i < n; i++) {
        size_t batch = each;

        if (left > 0) {
            batch++;
            left--;
        }
        args[i] = (struct thread_arg){
            .id = i,
            .ch = ch,
            .from = from,
            .to = from + batch,
        };
        thread_create(&tids[i], fn, &args[i]);
        from += batch;
    }
}

void join_threads(const size_t n, thread_id *tids) {
    for (size_t i = 0; i < n; i++)
        thread_join(tids[i], NULL);
}

void test_chan(const size_t repeat,
               const size_t cap,
               const size_t total,
               const size_t n_readers, thread_fn reader_fn,
               const size_t n_writers, thread_fn writer_fn)
{
    if (n_readers > THREAD_MAX || n_writers > THREAD_MAX)
        errx(1, "too many threads to create");
    if (total > MSG_MAX)
        errx(1, "too many messages to send");

    struct chan *ch = chan_make(cap, malloc);
    if (!ch)
        errx(1, "fail to create channel");

    msg_total = total;
    for (size_t rep = 0; rep < repeat; rep++) {
        printf("cap=%zu readers=%zu writers=%zu msgs=%zu ... %zu/%zu\n",
                cap, n_readers, n_writers, msg_total,
                rep + 1, repeat);

        memset(msg_count, 0, sizeof(size_t) * msg_total);
        create_threads(n_readers, reader_fn, reader_args, reader_tids, ch);
        create_threads(n_writers, writer_fn, writer_args, writer_tids, ch);
        join_threads(n_readers, reader_tids);
        join_threads(n_writers, writer_tids);

        for (size_t i = 0; i < msg_total; i++)
            assert(msg_count[i] == 1);
    }

    chan_close(ch);
    free(ch);
}

void bench_chan(const size_t repeat,
                const size_t cap,
                const size_t total,
                const size_t n_readers, thread_fn reader_fn,
                const size_t n_writers, thread_fn writer_fn)
{
    if (n_readers > THREAD_MAX || n_writers > THREAD_MAX)
        errx(1, "too many threads to create");
    if (total > MSG_MAX)
        errx(1, "too many messages to send");

    struct chan *ch = chan_make(cap, malloc);
    if (!ch)
        errx(1, "fail to create channel");

    // XXX not the ideal way
    uint64_t ts = __rdtsc();
    for (size_t rep = 0; rep < repeat; rep++) {
        create_threads(n_readers, reader_fn, reader_args, reader_tids, ch);
        create_threads(n_writers, writer_fn, writer_args, writer_tids, ch);
        join_threads(n_readers, reader_tids);
        join_threads(n_writers, writer_tids);
    }
    printf("cap=%zu r=%zu w=%zu msg=%zu\t...\t%lf cycles/msg\n",
            cap, n_readers, n_writers, total,
            (__rdtsc() - ts) * 1.0 / repeat / total);

    chan_close(ch);
    free(ch);
}

int main(int argc, char **argv) {
    test_chan(1000, 0, 10000, 80, reader, 80, writer);
    test_chan(1000, 7, 10000, 80, reader, 80, writer);

    //bench_chan(10000, 0, 100, 1, reader, 1, writer);
    //bench_chan(10000, 0, 100, 2, reader, 2, writer);
    //bench_chan(10000, 0, 100, 4, reader, 4, writer);
    //bench_chan(10000, 0, 100, 8, reader, 8, writer);
    //bench_chan(10000, 0, 100, 16, reader, 16, writer);
    //bench_chan(10000, 0, 100, 32, reader, 32, writer);
    //bench_chan(10000, 0, 100, 64, reader, 64, writer);

    //bench_chan(10000, 20, 100, 1, reader, 1, writer);
    //bench_chan(10000, 20, 100, 2, reader, 2, writer);
    //bench_chan(10000, 20, 100, 4, reader, 4, writer);
    //bench_chan(10000, 20, 100, 8, reader, 8, writer);
    //bench_chan(10000, 20, 100, 16, reader, 16, writer);
    //bench_chan(10000, 20, 100, 32, reader, 32, writer);
    //bench_chan(10000, 20, 100, 64, reader, 64, writer);

    return 0;
}
