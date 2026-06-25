#include "diag.h"
#include <stdio.h>

volatile uint32_t diag_dma_cbs       = 0;
volatile uint32_t diag_samples_pushed = 0;
volatile uint32_t diag_sd_bytes      = 0;
volatile uint32_t diag_missed_bufs   = 0;

#define DIAG_INTERVAL_MS 1000u

void diag_poll(UART_HandleTypeDef *huart, uint32_t ring_level_bytes) {
    static uint32_t last_tick = 0;
    uint32_t now = HAL_GetTick();
    if (now - last_tick < DIAG_INTERVAL_MS) return;
    last_tick = now;

    char buf[96];
    int len = snprintf(buf, sizeof(buf),
        "!D,%lu,%lu,%lu,%lu,%lu,%lu\r\n",
        (unsigned long)now,
        (unsigned long)diag_dma_cbs,
        (unsigned long)diag_samples_pushed,
        (unsigned long)diag_sd_bytes,
        (unsigned long)diag_missed_bufs,
        (unsigned long)ring_level_bytes);
    /* Blocking send; ~3 ms at 250 kbps — safe between SD writes */
    HAL_UART_Transmit(huart, (uint8_t *)buf, len, 10);
}
