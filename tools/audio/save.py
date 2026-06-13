import sys
import wave
import signal
import serial
import numpy as np
from datetime import datetime

from audio.common import BATCH_SAMPLES, DCOffset, MicReader, wait_for_port, exp_mov_avg, BAUD_RATE
from audio.filters import make_notch_filter, make_highpass_filter, make_pipeline

DURATION = 5
TARGET_RATE = 16000


def save(
    port: str | None = None,
    baud: int = BAUD_RATE,
    duration: float = DURATION,
    output: str | None = None,
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
    dc_offset = DCOffset(value=0.0)
    pipeline = make_pipeline(
        make_notch_filter(50.0, TARGET_RATE),
        make_highpass_filter(30.0, TARGET_RATE, order=4),
    )

    def process(val: float) -> None:
        dc_offset.value = exp_mov_avg(dc_offset.value, val)
        samples.append(pipeline(val - dc_offset.value))

    samples: list[float] = []
    reader = MicReader(ser)
    last_seq: int | None = None
    last_val: float = 0.0
    end = time.perf_counter() + duration
    signal.signal(signal.SIGINT, lambda *_: ser.close())
    try:
        while time.perf_counter() < end:
            seq, batch = reader.read_blocking()
            if last_seq is not None and seq != last_seq + 1:
                gap = (seq - last_seq - 1) * BATCH_SAMPLES
                first_val = float(batch[0])
                for i in range(gap):
                    process(last_val + (first_val - last_val) * (i + 1) / (gap + 1))
            for val in batch:
                process(float(val))
            last_seq = seq
            last_val = float(batch[-1])
    finally:
        ser.close()

    actual_rate = len(samples) / duration
    print(f"Captured {len(samples)} samples at {actual_rate:.1f} Hz")

    arr = np.array(samples, dtype=np.float32) / (2**15)
    resampled = np.interp(
        np.linspace(0, len(arr), int(len(arr) * TARGET_RATE / actual_rate)),
        np.arange(len(arr)),
        arr,
    ).astype(np.float32)
    i16 = (resampled * 32767).astype(np.int16)

    with wave.open(output, 'w') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(TARGET_RATE)
        wf.writeframes(i16.tobytes())

    print(f"Saved {output}")


if __name__ == "__main__":
    save(
        port=sys.argv[1] if len(sys.argv) > 1 else None,
        output=sys.argv[2] if len(sys.argv) > 2 else None,
    )
