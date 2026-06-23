#include "sdcard.h"
#include "diskio_test.h"
#include "integer.h"
#include "mic.h"
#include "ringbuf.h"
#include <stdint.h>
#include <stdio.h>

static FRESULT sdcard_write_test(void) {
    FIL file;
    UINT bw;
    FRESULT fr;

    fr = f_open(&file, "test.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK) return fr;

    fr = f_write(&file, "hello sd\n", 9, &bw);
    f_close(&file);
    return fr;
}

void sdcard_init(UART_HandleTypeDef *huart) {
    FRESULT fr = f_mount(&USERFatFS, USERPath, 1);
    if (fr != FR_OK) {
        char msg[32];
        int len = snprintf(msg, sizeof(msg), "SD mount FAIL: %d\r\n", fr);
        HAL_UART_Transmit(huart, (uint8_t *)msg, len, 100);
        return;
    }

    HAL_UART_Transmit(huart, (uint8_t *)"SD mount OK\r\n", 13, 100);

    fr = sdcard_write_test();
    if (fr == FR_OK) {
        HAL_UART_Transmit(huart, (uint8_t *)"SD write OK\r\n", 13, 100);
    } else {
        char msg[32];
        int len = snprintf(msg, sizeof(msg), "SD write FAIL: %d\r\n", fr);
        HAL_UART_Transmit(huart, (uint8_t *)msg, len, 100);
    }

#ifdef RUN_DISKIO_TESTS
    static DiskioTestResult diskio_test;
    diskio_test_run(&diskio_test);
#endif
}

FRESULT sdcard_close_recording(wav_recorder_t *wav) {
    if (!wav->is_open) return FR_OK;
    wav->is_open = false;
    return f_close(&wav->file);
}

FRESULT sdcard_open_recording(wav_recorder_t *wav) {
    static uint32_t index = 0;
    char filename[20];
    sdcard_close_recording(wav);
    ring_buf_flush(wav->buf);
    wav->data_bytes         = 0;
    wav->total_bufs         = 0;
    wav->missed_bufs        = 0;
    wav->sectors_since_sync = 0;
    snprintf(filename, sizeof(filename), "rec%03lu.bin", index++);
    FRESULT fr = f_open(&wav->file, filename, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr == FR_OK) wav->is_open = true;
    return fr;
}

/* Sync every 16 sectors (8 KB) to bound data loss on unclean eject */
#define SYNC_INTERVAL 16

FRESULT sdcard_drain(wav_recorder_t *wav) {
    if (!wav->is_open) return FR_OK;
    static uint8_t sector[CHUNK_SIZE];
    FRESULT fr = FR_OK;
    while (ring_buf_count(wav->buf) >= CHUNK_SIZE) {
        UINT bw;
        ring_buf_pop(wav->buf, sector, CHUNK_SIZE);
        fr = f_write(&wav->file, sector, CHUNK_SIZE, &bw);
        if (fr != FR_OK) return fr;
        wav->data_bytes += bw;
        if (++wav->sectors_since_sync >= SYNC_INTERVAL) {
            wav->sectors_since_sync = 0;
            fr = f_sync(&wav->file);
            if (fr != FR_OK) return fr;
        }
    }
    return fr;
}