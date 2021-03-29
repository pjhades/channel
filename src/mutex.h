#pragma once

#include <stdint.h>

struct mutex {
    uint32_t val;
};

#define MUTEX_INITIALIZER (struct mutex){.val = 0}

void mutex_init(struct mutex *mu);
void mutex_unlock(struct mutex *mu);
void mutex_lock(struct mutex *mu);
int mutex_trylock(struct mutex *mu);
