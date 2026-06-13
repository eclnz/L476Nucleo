import sys
import wave
import signal
import serial
import numpy as np
from datetime import datetime

from audio.common import wait_for_port, exp_mov_avg, BAUD_RATE, read_mic_sample

DURATION = 5


def save(
    port: str | None = None,
    baud: int = BAUD_RATE,
    duration: float = DURATION,
    output: str | None = None,
    target_rate: int = 16000,
) -> None:
    if port is None:
        port = wait_for_port()
        if port is None:
            print("No device found")
            return

    if output is None:
        output = datetime.now().strftime('stream_%Y%m%d_%H%M%S.wav')
    print(f"Connecting to {port}...")
    ser = serial.Serial(port, baud, timeout=1)
    print(f"Recording {duration}s...")

    import time
    dc = 0.0
    samples: list[float] = []
    end = time.perf_counter() + duration
    signal.signal(signal.SIGINT, lambda *_: ser.close())
    try:
        while time.perf_counter() < end:
            val = read_mic_sample(ser)
            dc = exp_mov_avg(dc, val)
            samples.append(val - dc)
    finally:
        ser.close()

    actual_rate = len(samples) / duration
    print(f"Captured {len(samples)} samples at {actual_rate:.1f} Hz")

    arr = np.array(samples, dtype=np.float32)
    resampled = np.interp(
        np.linspace(0, len(arr), int(len(arr) * target_rate / actual_rate)),
        np.arange(len(arr)),
        arr,
    )
    peak = np.max(np.abs(resampled))
    if peak > 0:
        resampled /= peak
    i16 = (resampled * 32767).astype(np.int16)

    with wave.open(output, 'w') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(target_rate)
        wf.writeframes(i16.tobytes())

    print(f"Saved {output}")


if __name__ == "__main__":
    save(
        port=sys.argv[1] if len(sys.argv) > 1 else None,
        output=sys.argv[2] if len(sys.argv) > 2 else None,
    )
