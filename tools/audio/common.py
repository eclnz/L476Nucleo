from dataclasses import dataclass, field
from typing import Iterator

import struct
import time
import glob
import serial


HP_ALPHA = 0.95  # high-pass filter coefficient — closer to 1.0 = lower cutoff frequency
BAUD_RATE = 1000000
BATCH_BYTES = 256 * 4  # one DMA half-buffer in bytes


@dataclass
class DCOffset:
    value: float = 0.0


@dataclass
class RateEstimator:
    count: int = 0
    start: float = field(init=False)

    def __post_init__(self) -> None:
        self.start = time.perf_counter()

    def add(self) -> None:
        self.count += 1

    def rate(self) -> float:
        elapsed = time.perf_counter() - self.start
        return self.count / elapsed if elapsed > 0 else 0.0


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


def read_mic_samples(ser: serial.Serial) -> Iterator[int]:
    while ser.in_waiting >= 4:
        raw = ser.read(4)
        if len(raw) < 4:
            break
        yield struct.unpack('<i', raw)[0] >> 8


def read_mic_sample(ser: serial.Serial) -> int:
    return struct.unpack('<i', ser.read(4))[0] >> 8


