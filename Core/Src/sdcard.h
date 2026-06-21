#ifndef __SDCARD_H
#define __SDCARD_H

#include "fatfs.h"
#include "stm32l4xx_hal.h"

void sdcard_init(UART_HandleTypeDef *huart);

#endif /* __SDCARD_H */
