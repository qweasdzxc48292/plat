#ifndef EDT_TIME_H
#define EDT_TIME_H

#include <stddef.h>
#include <stdint.h>

uint64_t edt_now_ns(void);
uint64_t edt_now_ms(void);
void edt_format_local_time(uint64_t timestamp_ns, char *out, size_t out_size);
int edt_sleep_ns(uint64_t duration_ns);

#endif
