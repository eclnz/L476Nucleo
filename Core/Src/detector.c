/**
  * @file           : detector.c
  * @brief          : Detector program body
  * @attention
  * Copyright (c) 2026 eclnz.
  * All rights reserved.
*/

#include "detector.h"

/* Sinc3 at oversampling=50 gives max |sample| ~125000 (50^3).
   A threshold of ~5000 distinguishes speech from mic noise floor. */
#define ATTACK_SHIFT  2   /* ~4-sample rise time */
#define DECAY_SHIFT   12  /* ~4096-sample fall time (~100ms at 40kHz) */

void detector_init(Detector *d, int32_t threshold)
{
    d->threshold = threshold;
    d->envelope  = 0;
    d->active    = false;
}

void detector_update(Detector *d, int32_t sample)
{
    int32_t level = sample < 0 ? -sample : sample;

    if (level > d->envelope)
        d->envelope += (level - d->envelope) >> ATTACK_SHIFT;
    else
        d->envelope -= d->envelope >> DECAY_SHIFT;

    d->active = d->envelope > d->threshold;
}

bool is_active(const Detector *d)
{
    return d->active;
}
