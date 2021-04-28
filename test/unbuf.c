#include "util.h"

struct unbuf_chan ch;

def_thread_fn(writer) {
    struct thread_arg *a = arg;
    for (size_t i = a->lo; i < a->hi; i++) {
        if (unbuf_chan_send(&ch, (void *)i) == -1)
            break;
    }
    return 0;
}

def_thread_fn(reader) {
    struct thread_arg *a = arg;
    size_t msg, received = 0, expect = a->hi - a->lo;
    while (received < expect) {
        if (unbuf_chan_recv(&ch, (void **)&msg) == -1)
            break;
        atomic_fetch_add_explicit(&msg_count[msg], 1, memory_order_relaxed);
        ++received;
    }
    return 0;
}

void setup(void) {
    ch = unbuf_chan_make();
}

void teardown(void) {
    unbuf_chan_close(&ch);
}

int main(int argc, char **argv) {
    test_chan(1, 10000, 20, reader, 20, writer, setup, teardown);
    return 0;
}
