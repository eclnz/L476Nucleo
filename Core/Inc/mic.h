#ifndef __MIC_H
#define __MIC_H

#include "ff.h"
#include "stm32l4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>
#include "ringbuf.h"

#define CHUNK_SIZE      512
#define AUDIO_BUF_HALF  CHUNK_SIZE
#define AUDIO_BUF_SIZE  (AUDIO_BUF_HALF * 2)
#define SYNC_START      0xABCDABCDU
#define SYNC_END        0xDCBADCBAU
#define tx_frame_SIZE   (4 + 4 + AUDIO_BUF_HALF * 2 + 4)
#define RINGBUF_SIZE    (CHUNK_SIZE * 16)

typedef enum  { MIC_BUF_EMPTY, MIC_BUF_HALF, MIC_BUF_FULL } MicBufferState;


typedef struct {
    volatile MicBufferState audio_read_ready;
    int32_t * volatile audio_half_buf_pos;
    uint32_t    frame_seq;                
    int32_t     audio_buf[AUDIO_BUF_SIZE];
    int32_t     process_buf[AUDIO_BUF_HALF];
    int16_t     ring_buf_data[RINGBUF_SIZE];
    ringbuf_t   ring_buf;
    uint8_t     tx_frame[tx_frame_SIZE];
} mic_t;

typedef struct {
    ringbuf_t   *buf;
    FIL         file;
    bool        is_open;
    uint32_t    data_bytes;
    uint32_t    sample_rate;
    uint32_t    total_bufs;
    uint32_t    missed_bufs;
    uint32_t    sectors_since_sync;
} wav_recorder_t;

typedef enum { MIC_READ_SUCC, MIC_READ_FAIL, MIC_READ_NOT_READY } MicReadOutc;

void        mic_init(mic_t *m);
void        transmit_audio(mic_t *m, UART_HandleTypeDef *huart);
MicReadOutc read_audio(mic_t *m, ringbuf_t *r);

#endif /* __MIC_H */