#include "sdcard.h"
#include "diskio_test.h"
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
