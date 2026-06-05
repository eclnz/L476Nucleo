import sys
import time
import signal
import serial
import numpy as np
import sounddevice as sd

from audio.common import DCOffset, wait_for_port, exp_mov_avg, BAUD_RATE

RECORD_SECONDS = 5


def record(ser: serial.Serial, duration: float) -> tuple[np.ndarray, float]:
    print(f"Recording {duration}s...")
    dc_offset = DCOffset(value=0.0)
    samples: list[int] = []
    end_time = time.perf_counter() + duration
    while time.perf_counter() < end_time:
        try:
            val = int(ser.readline().decode().strip())
        except ValueError:
            continue
        dc_offset.value = exp_mov_avg(dc_offset.value, val)
        samples.append(int(val - dc_offset.value))
    actual_rate = len(samples) / duration
    print(f"Captured {len(samples)} samples at {actual_rate:.1f} Hz")
    arr = np.array(samples, dtype=np.float32)
    return arr / np.max(np.abs(arr)), actual_rate


def main(
    port: str | None = None,
    baud: int = BAUD_RATE,
    duration: float = RECORD_SECONDS,
) -> None:
    if port is None:
        print("Waiting for ACM device...")
        port = wait_for_port()
        if port is None:
            print("Could not connect")
            return

    print(f"Connecting to {port}...")
    ser = serial.Serial(port, baud, timeout=1)
    print("Connected.")

    signal.signal(signal.SIGINT, lambda *_: ser.close())
    try:
        samples, actual_rate = record(ser, duration)
        target_rate = 16000
        resampled = np.interp(
            np.linspace(0, len(samples), int(len(samples) * target_rate / actual_rate)),
            np.arange(len(samples)),
            samples,
        ).astype(np.float32)
        print("Playing back...")
        sd.play(resampled, samplerate=target_rate)
        sd.wait()
        print("Done.")
    finally:
        ser.close()


if __name__ == "__main__":
    main(port=sys.argv[1] if len(sys.argv) > 1 else None)
