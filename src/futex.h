#pragma once

#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>

static inline long futex_wait(uint32_t *uaddr, uint32_t val) {
    return syscall(SYS_futex, uaddr, FUTEX_WAIT, val, NULL, NULL, 0);
}

static inline long futex_wake(uint32_t *uaddr, uint32_t val) {
    return syscall(SYS_futex, uaddr, FUTEX_WAKE, val, NULL, NULL, 0);
}
