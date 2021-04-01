#pragma once

#ifdef __STDC_NO_ATOMICS__
#error "no atomic library is available"
#else
#include <stdatomic.h>
#endif
