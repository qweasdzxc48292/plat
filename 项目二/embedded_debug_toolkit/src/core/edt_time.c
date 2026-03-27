#define _POSIX_C_SOURCE 200809L

#include "edt_time.h"

#include <inttypes.h>
#include <stdio.h>
#include <time.h>

uint64_t edt_now_ns(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0U;
    }
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

uint64_t edt_now_ms(void)
{
    return edt_now_ns() / 1000000ULL;
}

void edt_format_local_time(uint64_t timestamp_ns, char *out, size_t out_size)
{
    if (!out || out_size == 0U) {
        return;
    }
    (void)snprintf(out,
                   out_size,
                   "%" PRIu64 ".%06" PRIu64,
                   timestamp_ns / 1000000000ULL,
                   (timestamp_ns % 1000000000ULL) / 1000ULL);
}

int edt_sleep_ns(uint64_t duration_ns)
{
    struct timespec req;
    req.tv_sec = (time_t)(duration_ns / 1000000000ULL);
    req.tv_nsec = (long)(duration_ns % 1000000000ULL);
    while (nanosleep(&req, &req) != 0) {
        if (req.tv_sec == 0 && req.tv_nsec == 0) {
            break;
        }
    }
    return 0;
}
