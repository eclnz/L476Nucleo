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

static void test_push_basic(void) {
    uint8_t backing[8];
    ringbuf_t rb = { .data = backing, .capacity = 8 };

    assert(ring_buf_push(&rb, (uint8_t *)"ABCD", 4) == RB_PUSH_SUCC);
    assert(memcmp(backing, "ABCD", 4) == 0);
    assert(ring_buf_count(&rb) == 4);
    assert(ring_buf_free(&rb) == 4);
}

static void test_push_wrap_around(void) {
    uint8_t backing[8];
    ringbuf_t rb = { .data = backing, .capacity = 8, .head_count = 6, .tail_count = 6 };

    assert(ring_buf_push(&rb, (uint8_t *)"XYZW", 4) == RB_PUSH_SUCC);
    assert(backing[6] == 'X' && backing[7] == 'Y');
    assert(backing[0] == 'Z' && backing[1] == 'W');
}

static void test_push_overflow(void) {
    uint8_t backing[8];
    ringbuf_t rb = { .data = backing, .capacity = 8 };

    assert(ring_buf_push(&rb, (uint8_t *)"123456789", 9) == RB_PUSH_MISS);
    assert(ring_buf_count(&rb) == 0);
}

static void test_push_fill_exactly(void) {
    uint8_t backing[8];
    ringbuf_t rb = { .data = backing, .capacity = 8 };

    assert(ring_buf_push(&rb, (uint8_t *)"ABCDEFGH", 8) == RB_PUSH_SUCC);
    assert(ring_buf_free(&rb) == 0);
    assert(ring_buf_push(&rb, (uint8_t *)"X", 1) == RB_PUSH_MISS);
}

static void test_pop_basic(void) {
    uint8_t backing[8];
    uint8_t dst[4];
    ringbuf_t rb = { .data = backing, .capacity = 8 };

    ring_buf_push(&rb, (uint8_t *)"ABCD", 4);
    assert(ring_buf_pop(&rb, dst, 4) == RB_POP_SUCC);
    assert(memcmp(dst, "ABCD", 4) == 0);
    assert(ring_buf_count(&rb) == 0);
    assert(ring_buf_free(&rb) == 8);
}

static void test_pop_wrap_around(void) {
    uint8_t backing[8];
    uint8_t dst[4];
    ringbuf_t rb = { .data = backing, .capacity = 8, .head_count = 6, .tail_count = 6 };

    ring_buf_push(&rb, (uint8_t *)"XYZW", 4);
    assert(ring_buf_pop(&rb, dst, 4) == RB_POP_SUCC);
    assert(memcmp(dst, "XYZW", 4) == 0);
    assert(ring_buf_count(&rb) == 0);
}

static void test_pop_underflow(void) {
    uint8_t backing[8];
    uint8_t dst[4];
    ringbuf_t rb = { .data = backing, .capacity = 8 };

    ring_buf_push(&rb, (uint8_t *)"AB", 2);
    assert(ring_buf_pop(&rb, dst, 4) == RB_POP_MISS);
    assert(ring_buf_count(&rb) == 2);
}

static void test_push_pop_interleaved(void) {
    uint8_t backing[8];
    uint8_t dst[4];
    ringbuf_t rb = { .data = backing, .capacity = 8 };

    ring_buf_push(&rb, (uint8_t *)"AAAA", 4);
    ring_buf_pop(&rb, dst, 4);
    ring_buf_push(&rb, (uint8_t *)"BBBB", 4);
    ring_buf_pop(&rb, dst, 4);
    assert(memcmp(dst, "BBBB", 4) == 0);
    assert(ring_buf_count(&rb) == 0);
}

int main(void) {
    test_push_basic();
    test_push_wrap_around();
    test_push_overflow();
    test_push_fill_exactly();
    test_pop_basic();
    test_pop_wrap_around();
    test_pop_underflow();
    test_push_pop_interleaved();
    printf("All tests passed.\n");
    return 0;
}
