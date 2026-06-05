from dataclasses import dataclass

import time
import glob


HP_ALPHA = 0.95  # high-pass filter coefficient — closer to 1.0 = lower cutoff frequency
SAMPLE_RATE = 16000  # Hz — PCLK2 (4MHz) / DFSDM clock divider (2) / oversampling (125)
BAUD_RATE = 250000

@dataclass
class DCOffset:
    value: float = 0.0


def wait_for_port() -> str | None:
    tries = 30
    wait = 0.5
    print(f"Waiting for {tries}s")
    for _ in range(int(tries / wait)):
        ports = glob.glob("/dev/ttyACM*")
        if ports:
            return ports[0]
        time.sleep(wait)
    return None


def exp_mov_avg(dc_offset: float, val: float, alpha: float = HP_ALPHA) -> float:
    return alpha * dc_offset + (1 - alpha) * val
