#include "mic.h"
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
static void int32_buff_to_16(const int32_t *src, int16_t *dst, int len) {
    for (int i = 0; i < len; i++) {
        dst[i] = (int16_t)(src[i] >> 10);
    }
}

/**
 * @brief Build and transmit a framed audio packet over UART DMA.
 * @param m     Mic instance containing audio buffers and state.
 * @param huart UART handle to transmit on.
 * @note  Skipped if UART is still busy with the previous frame.
 */
void transmit_audio(Mic *m, UART_HandleTypeDef *huart){
    if (m->audio_read_ready != MIC_BUF_EMPTY && HAL_UART_GetState(huart) == HAL_UART_STATE_READY) {
        m->audio_read_ready = MIC_BUF_EMPTY;
        memcpy(m->process_buf, m->half_buf_pos, sizeof(m->process_buf));
        memcpy(m->tx_frame, &(uint32_t){SYNC_START}, 4);
        memcpy(m->tx_frame + 4, &m->frame_seq, 4);
        m->frame_seq++;
        int32_buff_to_16(m->process_buf, (int16_t *)(m->tx_frame + 8), AUDIO_BUF_HALF);
        memcpy(m->tx_frame + 8 + AUDIO_BUF_HALF * 2, &(uint32_t){SYNC_END}, 4);
        HAL_UART_Transmit_DMA(huart, m->tx_frame, sizeof(m->tx_frame));
    }
}
