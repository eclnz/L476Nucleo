## Voltage regulation (3.3v)

Voltage regulator: LD39050PU33R
- dropout voltage:          200 mV @ 500 mA
- power supply rejection:   65dB @ 10kHz
- output voltage tolerance: +-2%

## Current draw:

### Chip STM32L476RGT6 (from NUCLEO-L476RG)
current consumption (per MHz unless noted):
- Run (MR range 1):     112 μA/MHz  → ~3.6 mA @ 32 MHz
- Run (MR range 2):     100 μA/MHz
- Sleep (MR range 1):    37 μA/MHz
- Sleep (MR range 2):    35 μA/MHz
- Stop 0:               108 μA
- Stop 1:               6.6 μA (w/o RTC), 6.9 μA (w/ RTC)
- Stop 2:               1.1 μA (w/o RTC), 1.4 μA (w/ RTC)
- Standby:              0.35 μA (w/o RTC), 0.65 μA (w/ RTC)
- Shutdown:             0.03 μA (w/o RTC), 0.33 μA (w/ RTC)

### Adafruit 3492 — MP34DT01-M PDM Breakout
required clock: 2.4 MHz
current consumption:
- normal mode: 0.6 mA


### SD Card Module - XC4386
current consumption (estimated values)
- idle / standby:   ~0.2–1 mA
- reading:          ~20–40 mA typical
- writing / erasing: 50–100 mA, (can spike to 150–200)

source: https://envistiamall.com/products/micro-sd-tf-memory-card-reader-module-with-spi-interface


### Clock

power input: 2.3V - 5.5V
current consumption: < 300uA

source: https://envistiamall.com/blogs/learn/ds3231-at24c32-real-time-clock-module-user-guide

## Current budget summary (regulator max: 500 mA)

| Condition        | MCU   | Mic    | SD card     | Clock  | Total      |
|------------------|-------|--------|-------------|--------|------------|
| Idle             | 3.6   | 0.6    | ~0.5        | 0.3    | ~5 mA      |
| SD reading       | 3.6   | 0.6    | ~30         | 0.3    | ~34 mA     |
| SD writing       | 3.6   | 0.6    | ~75         | 0.3    | ~80 mA     |
| SD write (spike) | 3.6   | 0.6    | up to 200   | 0.3    | ~205 mA    |