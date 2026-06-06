import sys
import time
import signal
import serial
import numpy as np
import matplotlib.pyplot as plt

from audio.common import DCOffset, wait_for_port, exp_mov_avg, BAUD_RATE

DURATION = 5


def main(port: str | None = None, baud: int = BAUD_RATE, duration: float = DURATION) -> None:
    if port is None:
        port = wait_for_port()
        if port is None:
            print("No device found")
            return

    print(f"Connecting to {port}...")
    ser = serial.Serial(port, baud, timeout=1)
    dc = DCOffset()
    warmup = 0
    while warmup < 50:
        try:
            val = int(ser.readline().decode().strip())
        except ValueError:
            continue
        dc.value = exp_mov_avg(dc.value, val)
        warmup += 1

    ser.reset_input_buffer()
    print(f"Capturing {duration}s...")

    samples: list[float] = []
    end = time.perf_counter() + duration
    signal.signal(signal.SIGINT, lambda *_: ser.close())
    try:
        while time.perf_counter() < end:
            try:
                val = int(ser.readline().decode().strip())
            except ValueError:
                continue
            dc.value = exp_mov_avg(dc.value, val)
            samples.append(val - dc.value)
    finally:
        ser.close()

    actual_rate = len(samples) / duration
    print(f"Captured {len(samples)} samples at {actual_rate:.1f} Hz")

    arr = np.array(samples)
    fig, axes = plt.subplots(2, 1, figsize=(12, 8))

    axes[0].plot(np.linspace(0, duration, len(arr)), arr)
    axes[0].set_title(f"{len(arr)} samples at {actual_rate:.1f} Hz — RMS {np.sqrt(np.mean(arr**2)):.1f}")
    axes[0].set_xlabel("Time (s)")
    axes[0].set_ylabel("Amplitude")

    axes[1].specgram(arr, Fs=actual_rate, cmap='inferno')
    axes[1].set_title("Spectrogram")
    axes[1].set_ylabel("Frequency (Hz)")
    axes[1].set_xlabel("Time (s)")

    plt.tight_layout()
    plt.show()


if __name__ == '__main__':
    main(port=sys.argv[1] if len(sys.argv) > 1 else None)
