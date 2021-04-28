#include "util.h"

thread_id reader_tids[THREAD_MAX];
thread_id writer_tids[THREAD_MAX];
struct thread_arg reader_args[THREAD_MAX];
struct thread_arg writer_args[THREAD_MAX];
_Atomic size_t msg_total, msg_count[MSG_MAX];

static void create_threads(const size_t n,
                           thread_fn fn,
                           struct thread_arg *args,
                           thread_id *tids)
{
    size_t each = msg_total / n;
    size_t left = msg_total % n;
    size_t lo = 0;

    for (size_t i = 0; i < n; i++) {
        size_t batch = each;

        if (left > 0) {
            batch++;
            left--;
        }
        args[i] = (struct thread_arg){
            .id = i,
            .lo = lo,
            .hi = lo + batch,
        };
        thread_create(&tids[i], fn, &args[i]);
        lo += batch;
    }
}

static void join_threads(const size_t n, thread_id *tids) {
    for (size_t i = 0; i < n; i++)
        thread_join(tids[i], NULL);
}

void test_chan(const size_t repeat,
               const size_t total,
               const size_t n_readers, thread_fn reader_fn,
               const size_t n_writers, thread_fn writer_fn,
               setup_fn setup, teardown_fn teardown)
{
    if (n_readers > THREAD_MAX || n_writers > THREAD_MAX)
        errx(1, "too many threads to create");
    if (total > MSG_MAX)
        errx(1, "too many messages to send");

    setup();

    msg_total = total;
    for (size_t rep = 0; rep < repeat; rep++) {
        printf("[%s] readers=%zu writers=%zu msgs=%zu ... %zu/%zu\n",
                __FILE__,
                n_readers, n_writers, msg_total,
                rep + 1, repeat);

        memset(msg_count, 0, sizeof(size_t) * msg_total);
        create_threads(n_readers, reader_fn, reader_args, reader_tids);
        create_threads(n_writers, writer_fn, writer_args, writer_tids);
        join_threads(n_readers, reader_tids);
        join_threads(n_writers, writer_tids);

        for (size_t i = 0; i < msg_total; i++)
            assert(msg_count[i] == 1);
    }

    teardown();
}
