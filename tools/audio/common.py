from dataclasses import dataclass, field
from typing import Iterator

import struct
import time
import glob
import serial


HP_ALPHA = 0.95  # high-pass filter coefficient — closer to 1.0 = lower cutoff frequency
BAUD_RATE = 1000000
BATCH_SAMPLES = 256
BATCH_BYTES = BATCH_SAMPLES * 4
SYNC_START = struct.pack('<I', 0xABCDABCD)
SYNC_END   = struct.pack('<I', 0xDCBADCBA)
FRAME_BYTES = 4 + BATCH_BYTES + 4


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


class MicReader:
    def __init__(self, ser: serial.Serial) -> None:
        self._ser = ser
        self._buf = bytearray()

    def read(self) -> Iterator[int]:
        self._buf += self._ser.read(self._ser.in_waiting)
        if len(self._buf) < FRAME_BYTES:
            return
        while True:
            idx = self._buf.find(SYNC_START)
            if idx == -1:
                self._buf = self._buf[-3:]  # keep tail in case marker spans reads
                return
            self._buf = self._buf[idx:]  # discard bytes before start marker
            frame_end = 4 + BATCH_BYTES + 4
            if len(self._buf) < frame_end:
                return  # wait for more data
            if bytes(self._buf[4 + BATCH_BYTES:frame_end]) != SYNC_END:
                self._buf = self._buf[4:]  # bad frame, skip past this start marker
                continue
            data = bytes(self._buf[4:4 + BATCH_BYTES])
            self._buf = self._buf[frame_end:]
            for s in struct.unpack(f'<{BATCH_SAMPLES}i', data):
                yield s >> 8

    def read_blocking(self) -> Iterator[int]:
        while True:
            samples = list(self.read())
            if samples:
                yield from samples
                return
            needed = max(1, FRAME_BYTES - len(self._buf))
            self._buf += self._ser.read(needed)


def read_mic_sample(ser: serial.Serial) -> int:
    return struct.unpack('<i', ser.read(4))[0] >> 8


