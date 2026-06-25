#ifndef __DIAG_H
#define __DIAG_H

#include "stm32l4xx_hal.h"
#include <stdint.h>

/*
 * Cumulative counters incremented from ISR context — always volatile.
 * Never reset; the Python tool diffs consecutive readings to get rates.
 */
extern volatile uint32_t diag_dma_cbs;       /* DFSDM DMA half+full completions */
extern volatile uint32_t diag_samples_pushed; /* int16 samples pushed to ring    */
extern volatile uint32_t diag_sd_bytes;       /* bytes written to SD card        */
extern volatile uint32_t diag_missed_bufs;    /* ring-full drops                 */

/*
 * Call once per main-loop iteration.  Emits a diagnostic line over UART
 * at 1 Hz: "!D,<tick_ms>,<dma_cbs>,<samples_pushed>,<sd_bytes>,<missed_bufs>,<ring_bytes>\r\n"
 */
void diag_poll(UART_HandleTypeDef *huart, uint32_t ring_level_bytes);

#endif /* __DIAG_H */
