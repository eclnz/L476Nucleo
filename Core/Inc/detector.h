/**
  * @file           : detector.h
  * @brief          : Detector header
  * @attention
  * Copyright (c) 2026 eclnz.
  * All rights reserved.
*/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __DETECTOR_H
#define __DETECTOR_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int32_t threshold;
    int32_t envelope;
    bool active;
} Detector;

void detector_init(Detector *d, int32_t threshold);
void detector_update(Detector *d, int32_t sample);
bool is_active(const Detector *d);

#endif /* __DETECTOR_H */
