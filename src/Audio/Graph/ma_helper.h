#pragma once

#include <assert.h>
#include <string.h>

static MA_INLINE void ma_zero_memory_default(void *p, size_t sz) {
    if (p == NULL) {
        assert(sz == 0);
        return;
    }

    if (sz > 0) {
        memset(p, 0, sz);
    }
}

#define MA_ZERO_OBJECT(p) ma_zero_memory_default((p), sizeof(*(p)))
