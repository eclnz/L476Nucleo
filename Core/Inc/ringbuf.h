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
typedef enum { RB_POP_SUCC,  RB_POP_MISS  } RingBufPopOutc;

uint32_t        ring_buf_count(const ringbuf_t *rb);
uint32_t        ring_buf_free(const ringbuf_t *rb);
RingBufPushOutc ring_buf_push(ringbuf_t *rb, const uint8_t *src, uint32_t len);
RingBufPopOutc  ring_buf_pop(ringbuf_t *rb, uint8_t *dst, uint32_t len);
void            ring_buf_flush(ringbuf_t *rb);

#endif /* __RINGBUF_H */
