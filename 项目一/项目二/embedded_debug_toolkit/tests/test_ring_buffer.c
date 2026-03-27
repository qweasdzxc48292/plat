#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "ring_buffer.h"

int test_ring_buffer(void)
{
    edt_ring_buffer_t rb;
    uint8_t in1[] = {1, 2, 3, 4, 5, 6};
    uint8_t out[16];
    size_t n;

    assert(edt_ring_buffer_init(&rb, 8U) == 0);

    n = edt_ring_buffer_write(&rb, in1, sizeof(in1));
    assert(n == sizeof(in1));
    assert(edt_ring_buffer_available_data(&rb) == sizeof(in1));

    n = edt_ring_buffer_read(&rb, out, 3U);
    assert(n == 3U);
    assert(memcmp(out, in1, 3U) == 0);

    {
        uint8_t in2[] = {7, 8, 9, 10, 11};
        n = edt_ring_buffer_write(&rb, in2, sizeof(in2));
        assert(n == sizeof(in2));
    }

    n = edt_ring_buffer_read(&rb, out, sizeof(out));
    assert(n == 8U);
    {
        uint8_t expect[] = {4, 5, 6, 7, 8, 9, 10, 11};
        assert(memcmp(out, expect, sizeof(expect)) == 0);
    }

    edt_ring_buffer_destroy(&rb);
    return 0;
}
