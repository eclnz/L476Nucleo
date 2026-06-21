/**
 * @file ringbuf.c
 * @brief Lock-free ring buffer for byte streaming between DMA and application.
 *
 * Uses monotonically increasing head/tail counters so index wrapping is a
 * single bitwise AND — valid only when capacity is a power of two.
 */

#include "ringbuf.h"
#include <string.h>

#ifndef __DMB
#define __DMB() __asm__ volatile("" ::: "memory")
#endif

/**
 * @brief Returns the number of bytes currently stored in the buffer.
 * @param rb Ring buffer instance.
 * @return Bytes available to read.
 */
uint32_t ring_buf_count(const ringbuf_t *rb) {
    return rb->head_count - rb->tail_count;
}

/**
 * @brief Returns the number of bytes that can still be written.
 * @param rb Ring buffer instance.
 * @return Bytes available to write.
 */
uint32_t ring_buf_free(const ringbuf_t *rb) {
    return rb->capacity - ring_buf_count(rb);
}

/**
 * @brief Copies @p len bytes from @p src into the ring buffer.
 *
 * Splits the write into two memcpy calls when the data crosses the end of
 * the backing array. A data memory barrier is issued before updating
 * head_count so the consumer never sees a stale pointer.
 *
 * @param rb  Ring buffer instance.
 * @param src Data to write.
 * @param len Number of bytes to write.
 * @return RB_PUSH_SUCC on success, RB_PUSH_MISS if insufficient space.
 */
RingBufPushOutc ring_buf_push(ringbuf_t *rb, const uint8_t *src, uint32_t len) {
    if (len > ring_buf_free(rb)) {
        return RB_PUSH_MISS;
    }
    uint32_t write_idx    = rb->head_count & (rb->capacity - 1);
    uint32_t bytes_to_end = rb->capacity - write_idx;
    uint32_t first_chunk  = (bytes_to_end < len) ? bytes_to_end : len;

    memcpy(&rb->data[write_idx], src,               first_chunk);
    memcpy(&rb->data[0],         src + first_chunk, len - first_chunk);
    __DMB();
    rb->head_count += len;
    return RB_PUSH_SUCC;
}
