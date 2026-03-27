#ifndef EDT_COMMON_H
#define EDT_COMMON_H

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define EDT_MAX_CHANNELS 8U
#define EDT_MAX_SAMPLE_RATE_HZ 100000000U

#define EDT_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static inline const char *edt_safe_strerror(int err, char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0U) {
        return "invalid buffer";
    }
#if defined(_GNU_SOURCE) && !defined(__APPLE__)
    return strerror_r(err, buf, buf_size);
#else
    if (strerror_r(err, buf, buf_size) != 0) {
        (void)snprintf(buf, buf_size, "errno=%d", err);
    }
    return buf;
#endif
}

#endif
