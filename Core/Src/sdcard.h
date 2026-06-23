#ifndef __SDCARD_H
#define __SDCARD_H

#include "fatfs.h"
#include "ff.h"
#include "mic.h"
#include "ringbuf.h"
#include "stm32l4xx_hal.h"

void    sdcard_init(UART_HandleTypeDef *huart);
FRESULT sdcard_open_recording(wav_recorder_t *wav);
FRESULT sdcard_drain(wav_recorder_t *wav);

#endif /* __SDCARD_H */
