#ifndef __RINGBUF_H
#define __RINGBUF_H

#include <stdint.h>

typedef struct {
    uint8_t     *data;
    uint32_t    capacity;
    volatile uint32_t head_count;
    volatile uint32_t tail_count;
} ringbuf_t;

typedef enum { RB_PUSH_SUCC, RB_PUSH_MISS } RingBufPushOutc;

uint32_t       ring_buf_count(const ringbuf_t *rb);
uint32_t       ring_buf_free(const ringbuf_t *rb);
RingBufPushOutc ring_buf_push(ringbuf_t *rb, const uint8_t *src, uint32_t len);

#endif /* __RINGBUF_H */
