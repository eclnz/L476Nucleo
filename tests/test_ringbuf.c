/**
 * @file test_ringbuf.c
 * @brief Host-side unit tests for ringbuf_t push, count, and free operations.
 *
 * Compile and run with: make run (from the tests/ directory)
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../Core/Src/ringbuf.h"

static void test_basic_push(void) {
    uint8_t backing[8];
    ringbuf_t rb = { .data = backing, .capacity = 8 };

    assert(ring_buf_push(&rb, (uint8_t *)"ABCD", 4) == RB_PUSH_SUCC);
    assert(memcmp(backing, "ABCD", 4) == 0);
    assert(ring_buf_count(&rb) == 4);
    assert(ring_buf_free(&rb) == 4);
}

static void test_wrap_around(void) {
    uint8_t backing[8];
    ringbuf_t rb = { .data = backing, .capacity = 8, .head_count = 6, .tail_count = 6 };

    assert(ring_buf_push(&rb, (uint8_t *)"XYZW", 4) == RB_PUSH_SUCC);
    assert(backing[6] == 'X' && backing[7] == 'Y');
    assert(backing[0] == 'Z' && backing[1] == 'W');
}

static void test_overflow(void) {
    uint8_t backing[8];
    ringbuf_t rb = { .data = backing, .capacity = 8 };

    assert(ring_buf_push(&rb, (uint8_t *)"123456789", 9) == RB_PUSH_MISS);
    assert(ring_buf_count(&rb) == 0);
}

static void test_fill_exactly(void) {
    uint8_t backing[8];
    ringbuf_t rb = { .data = backing, .capacity = 8 };

    assert(ring_buf_push(&rb, (uint8_t *)"ABCDEFGH", 8) == RB_PUSH_SUCC);
    assert(ring_buf_free(&rb) == 0);
    assert(ring_buf_push(&rb, (uint8_t *)"X", 1) == RB_PUSH_MISS);
}

int main(void) {
    test_basic_push();
    test_wrap_around();
    test_overflow();
    test_fill_exactly();
    printf("All tests passed.\n");
    return 0;
}
