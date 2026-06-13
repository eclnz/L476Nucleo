import sys
import time
import signal
import serial
import numpy as np
import sounddevice as sd

from audio.common import BATCH_SAMPLES, DCOffset, MicReader, wait_for_port, exp_mov_avg, BAUD_RATE
from audio.filters import make_notch_filter, make_highpass_filter, make_pipeline

RECORD_SECONDS = 5


def record(ser: serial.Serial, duration: float) -> tuple[np.ndarray, float]:
    print(f"Recording {duration}s...")
    dc_offset = DCOffset(value=0.0)
    pipeline = make_pipeline(
        make_notch_filter(50.0, 16000.0),
        make_highpass_filter(30.0, 16000.0, order=4),
    )
    def process(val: float) -> None:
        dc_offset.value = exp_mov_avg(dc_offset.value, val)
        samples.append(pipeline(val - dc_offset.value))

    samples: list[float] = []
    reader = MicReader(ser)
    end_time = time.perf_counter() + duration
    last_seq: int | None = None
    last_val: float = 0.0
    while time.perf_counter() < end_time:
        seq, batch = reader.read_blocking()
        if last_seq is not None and seq != last_seq + 1:
            missed = seq - last_seq - 1
            gap = missed * BATCH_SAMPLES
            first_val = float(batch[0])
            for i in range(gap):
                process(last_val + (first_val - last_val) * (i + 1) / (gap + 1))
        for val in batch:
            process(float(val))
        last_seq = seq
        last_val = float(batch[-1])
    actual_rate = len(samples) / duration
    print(f"Captured {len(samples)} samples at {actual_rate:.1f} Hz")
    arr = np.array(samples, dtype=np.float32)  # already DC-removed and filtered
    return arr / (2**15), actual_rate


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
