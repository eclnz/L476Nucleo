#include "sdcard.h"
#include "diskio_test.h"
#include "integer.h"
#include "mic.h"
#include "ringbuf.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

/* Header is padded to 512 bytes with a JUNK chunk so audio data starts on a
   sector boundary, keeping every subsequent write sector-aligned. Layout:
     0–11   RIFF descriptor  (12 bytes)
    12–35   fmt sub-chunk    (24 bytes)
    36–43   JUNK sub-chunk header — "JUNK" + 460  (8 bytes)
    44–503  JUNK data        (460 zero bytes)
   504–511  data sub-chunk header — "data" + placeholder  (8 bytes)
   512+     audio samples */
#define WAV_HEADER_SIZE 512
#define JUNK_DATA_SIZE  460

static FRESULT write_wav_header(FIL *file, uint32_t sample_rate) {
    uint8_t hdr[WAV_HEADER_SIZE];
    uint32_t byte_rate    = sample_rate * 2;
    uint32_t fmt_size     = 16;
    uint32_t junk_size    = JUNK_DATA_SIZE;
    uint32_t placeholder  = 0;
    uint16_t audio_fmt    = 1, channels = 1, block_align = 2, bits = 16;

    memset(hdr, 0, sizeof(hdr));
    memcpy(&hdr[0],   "RIFF",       4);
    memcpy(&hdr[4],   &placeholder, 4);
    memcpy(&hdr[8],   "WAVE",       4);
    memcpy(&hdr[12],  "fmt ",       4);
    memcpy(&hdr[16],  &fmt_size,    4);
    memcpy(&hdr[20],  &audio_fmt,   2);
    memcpy(&hdr[22],  &channels,    2);
    memcpy(&hdr[24],  &sample_rate, 4);
    memcpy(&hdr[28],  &byte_rate,   4);
    memcpy(&hdr[32],  &block_align, 2);
    memcpy(&hdr[34],  &bits,        2);
    memcpy(&hdr[36],  "JUNK",       4);
    memcpy(&hdr[40],  &junk_size,   4);
    /* hdr[44..503] already zeroed by memset */
    memcpy(&hdr[504], "data",       4);
    memcpy(&hdr[508], &placeholder, 4);

    UINT bw;
    return f_write(file, hdr, sizeof(hdr), &bw);
}

static FRESULT finalize_wav_header(FIL *file, uint32_t data_bytes) {
    /* RIFF chunk size = total file size - 8; total = WAV_HEADER_SIZE + data_bytes */
    uint32_t riff_size = data_bytes + WAV_HEADER_SIZE - 8;
    UINT bw;
    FRESULT fr;

    fr = f_lseek(file, 4);
    if (fr != FR_OK) return fr;
    fr = f_write(file, &riff_size, 4, &bw);
    if (fr != FR_OK) return fr;

    fr = f_lseek(file, 508);
    if (fr != FR_OK) return fr;
    return f_write(file, &data_bytes, 4, &bw);
}

FRESULT sdcard_close_recording(wav_recorder_t *wav) {
    if (!wav->is_open) return FR_OK;

    FRESULT fr = sdcard_drain(wav);
    if (fr == FR_OK) {
        uint32_t remaining = ring_buf_count(wav->buf);
        if (remaining > 0) {
            static uint8_t tail[CHUNK_SIZE];
            ring_buf_pop(wav->buf, tail, remaining);
            UINT bw;
            fr = f_write(&wav->file, tail, remaining, &bw);
            if (fr == FR_OK) wav->data_bytes += bw;
        }
    }

    if (fr == FR_OK)
        fr = finalize_wav_header(&wav->file, wav->data_bytes);

    wav->is_open = false;
    FRESULT close_fr = f_close(&wav->file);
    return fr != FR_OK ? fr : close_fr;
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
    wav->sample_rate        = 40000;
    snprintf(filename, sizeof(filename), "rec%03lu.wav", index++);
    FRESULT fr = f_open(&wav->file, filename, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK) return fr;
    fr = write_wav_header(&wav->file, wav->sample_rate);
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