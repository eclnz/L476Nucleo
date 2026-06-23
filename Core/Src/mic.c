#include "mic.h"
#include "ringbuf.h"
#include "stm32l4xx_hal_conf.h"
#include "stm32l4xx_hal_uart.h"
#include <stdint.h>
#include <string.h>

/**
 * @brief Convert 32-bit DFSDM samples to 16-bit PCM by right-shifting 10 bits.
 * @param src Input buffer of 32-bit DFSDM samples.
 * @param dst Output buffer of 16-bit PCM samples.
 * @param len Number of samples to convert.
 */
void mic_init(mic_t *m) {
    m->ring_buf.data     = (uint8_t *)m->ring_buf_data;
    m->ring_buf.capacity = sizeof(m->ring_buf_data);
}

static void int32_buff_to_16(const int32_t *src, int16_t *dst, int len) {
    for (int i = 0; i < len; i++) {
        dst[i] = (int16_t)(src[i] >> 10);
    }
}

/**
 * @brief Build and transmit a framed audio packet over UART DMA.
 * @param m     mic_t instance containing audio buffers and state.
 * @param huart UART handle to transmit on.
 * @note  Skipped if UART is still busy with the previous frame.
 */
void transmit_audio(mic_t *m, UART_HandleTypeDef *huart){
    if (m->audio_read_ready != MIC_BUF_EMPTY && HAL_UART_GetState(huart) == HAL_UART_STATE_READY) {
        m->audio_read_ready = MIC_BUF_EMPTY;
        memcpy(m->process_buf, m->audio_half_buf_pos, sizeof(m->process_buf));
        /* [0-3] SYNC_START */
        memcpy(m->tx_frame, &(uint32_t){SYNC_START}, 4);
        /* [4-7] frame_seq */
        memcpy(m->tx_frame + 4, &m->frame_seq, 4);
        m->frame_seq++;
        /* [8-1031] PCM samples. Assuming buffer is 1024 long */
        int32_buff_to_16(m->process_buf, (int16_t *)(m->tx_frame + 8), AUDIO_BUF_HALF);
        /* [1032-1035] SYNC_END */
        memcpy(m->tx_frame + 1032, &(uint32_t){SYNC_END}, 4);
        HAL_UART_Transmit_DMA(huart, m->tx_frame, sizeof(m->tx_frame));
    }
}

MicWriteOutc read_audio(mic_t *m, ringbuf_t *r) {
    if (m->audio_read_ready == MIC_BUF_EMPTY) {
        return MIC_READ_NOT_READY;
    }
    m->audio_read_ready = MIC_BUF_EMPTY;
    memcpy(m->process_buf, m->audio_half_buf_pos, sizeof(m->process_buf));

    int16_t pcm[AUDIO_BUF_HALF];
    int32_buff_to_16(m->process_buf, pcm, AUDIO_BUF_HALF);

    if (ring_buf_push(r, (uint8_t *)pcm, sizeof(pcm)) == RB_PUSH_MISS) {
        return MIC_READ_FAIL;
    }
    return MIC_READ_SUCC;
}