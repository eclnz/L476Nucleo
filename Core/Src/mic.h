#ifndef __MIC_H
#define __MIC_H

#include "stm32l4xx_hal.h"
#include <stdint.h>

#define AUDIO_BUF_HALF 512
#define AUDIO_BUF_SIZE (AUDIO_BUF_HALF * 2)
#define SYNC_START     0xABCDABCDU
#define SYNC_END       0xDCBADCBAU
#define TX_FRAME_SIZE   (4 + 4 + AUDIO_BUF_HALF * 2 + 4)

typedef enum  { MIC_BUF_EMPTY, MIC_BUF_HALF, MIC_BUF_FULL } MicBufferState;

typedef struct {
    volatile MicBufferState audio_read_ready;
    int32_t * volatile half_buf_pos;
    uint32_t    frame_seq;
    int32_t     audio_buf[AUDIO_BUF_SIZE];
    int32_t     process_buf[AUDIO_BUF_HALF];
    uint8_t     tx_frame[TX_FRAME_SIZE];
} Mic;

void transmit_audio(Mic *m, UART_HandleTypeDef *huart);

#endif /* __MIC_H */