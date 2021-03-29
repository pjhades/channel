// Based on https://akkadia.org/drepper/futex.pdf

#include <stdint.h>
#include <stdatomic.h>
#include "futex.h"
#include "mutex.h"

enum {
    UNLOCKED = 0,
    LOCKED_NO_WAITER = 1,
    LOCKED = 2,
};

void mutex_init(struct mutex *mu) {
    mu->val = UNLOCKED;
}

void mutex_unlock(struct mutex *mu) {
    uint32_t orig = atomic_fetch_sub(&mu->val, 1);
    if (orig != LOCKED_NO_WAITER) {
        mu->val = UNLOCKED;
        futex_wake(&mu->val, 1);
    }
}

static uint32_t cas(uint32_t *ptr, uint32_t expect, uint32_t new) {
    atomic_compare_exchange_strong(ptr, &expect, new);
    return expect;
}

void mutex_lock(struct mutex *mu) {
    uint32_t val = cas(&mu->val, UNLOCKED, LOCKED_NO_WAITER);
    if (val != UNLOCKED) {
        do {
            if (val == LOCKED || cas(&mu->val, LOCKED_NO_WAITER, LOCKED) != UNLOCKED)
                futex_wait(&mu->val, LOCKED);
        } while ((val = cas(&mu->val, UNLOCKED, LOCKED)) != UNLOCKED);
    }
}

int mutex_trylock(struct mutex *mu) {
    uint32_t val = cas(&mu->val, UNLOCKED, LOCKED_NO_WAITER);
    return -!!(val != UNLOCKED);
}
