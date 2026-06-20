# TODO / Improvements

## Hardware

- [ ] Add bulk capacitor (e.g. 100µF) close to SD card module to absorb write spikes
- [ ] Investigate SD card module schematic to confirm onboard decoupling values

## Power

- [ ] Consider powering SD card module from 5V with a level shifter on MISO to protect STM32 GPIO — would isolate SD write spikes from the main 3.3V rail and allow the onboard AMS1117 to regulate properly

### Mic noise isolation (in order of effort)

- [ ] **Try first:** Power Adafruit 3492 from 5V via its VIN pin — onboard LDO provides locally regulated 3.3V, isolating mic from SD write spikes on the main 3.3V rail. PDM signals remain 3.3V so no STM32 compatibility issue.
- [ ] **Proper solution:** Dedicated low-noise LDO for mic supply (e.g. LP2985 or MCP1700), fed from main rail via a ferrite bead. Separate digital and analog rails entirely.
  - Ferrite bead on input to block high-frequency digital noise
  - Low-noise LDO (better PSRR at high frequency than AMS1117/LD39050)
  - Output decoupling: 100nF ceramic + 10µF bulk
  - Feed only the mic from this rail
