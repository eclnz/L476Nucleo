import sys
import time
import signal
import serial
import numpy as np
import matplotlib.pyplot as plt
from typing import Any

from audio.common import DCOffset, wait_for_port, exp_mov_avg, BAUD_RATE
from audio.filters import make_notch_filter, make_highpass_filter, make_pipeline

DURATION = 5


def dominant_freqs(arr: np.ndarray, rate: float, n: int = 10) -> list[tuple[Any, Any]]:
    freqs = np.fft.rfftfreq(len(arr), d=1.0 / rate)
    magnitudes = np.abs(np.fft.rfft(arr))
    peak_indices = np.argsort(magnitudes)[-n:][::-1]
    return [(freqs[i], magnitudes[i]) for i in peak_indices]


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

    raw: list[float] = []
    running_rms = 1.0
    spikes = 0
    end = time.perf_counter() + duration
    signal.signal(signal.SIGINT, lambda *_: ser.close())
    try:
        while time.perf_counter() < end:
            try:
                val = int(ser.readline().decode().strip())
            except ValueError:
                continue
            dc.value = exp_mov_avg(dc.value, val)
            clean = float(val) - dc.value
            running_rms = exp_mov_avg(running_rms, clean ** 2, alpha=0.999)
            if running_rms > 0 and abs(clean) > 10 * running_rms ** 0.5:
                spikes += 1
                continue
            raw.append(clean)
    finally:
        ser.close()

    if spikes:
        print(f"Rejected {spikes} spike samples")

    actual_rate = len(raw) / duration
    print(f"Captured {len(raw)} samples at {actual_rate:.1f} Hz")

    arr_raw = np.array(raw)
    pipeline = make_pipeline(
        make_notch_filter(50.0, actual_rate),
        make_highpass_filter(30.0, actual_rate, order=4),
    )
    arr_filtered = np.array([pipeline(x) for x in raw])

    peaks_raw = dominant_freqs(arr_raw, actual_rate)
    peaks_filtered = dominant_freqs(arr_filtered, actual_rate)

    print(f"\nWithout filter — RMS {np.sqrt(np.mean(arr_raw**2)):.1f}")
    for f, m in sorted(peaks_raw, key=lambda x: -x[1]):
        print(f"  {f:8.1f} Hz  magnitude {m:.0f}")

    print(f"\nWith 50Hz notch — RMS {np.sqrt(np.mean(arr_filtered**2)):.1f}")
    for f, m in sorted(peaks_filtered, key=lambda x: -x[1]):
        print(f"  {f:8.1f} Hz  magnitude {m:.0f}")

    freqs_raw = np.fft.rfftfreq(len(arr_raw), d=1.0 / actual_rate)
    mag_raw = np.abs(np.fft.rfft(arr_raw))
    mag_filtered = np.abs(np.fft.rfft(arr_filtered))

    fig, axes = plt.subplots(2, 1, figsize=(12, 7))
    fig.suptitle(f"50Hz notch filter comparison — {duration}s capture", fontsize=11)

    axes[0].plot(freqs_raw, mag_raw, linewidth=0.8, label=f"Raw  RMS={np.sqrt(np.mean(arr_raw**2)):.1f}", alpha=0.8)
    axes[0].plot(freqs_raw, mag_filtered, linewidth=0.8, label=f"Filtered  RMS={np.sqrt(np.mean(arr_filtered**2)):.1f}", alpha=0.8)
    axes[0].axvline(50, color='red', linestyle='--', linewidth=0.8, alpha=0.6, label='50 Hz')
    axes[0].set_title("Frequency spectrum")
    axes[0].set_xlabel("Frequency (Hz)")
    axes[0].set_ylabel("Magnitude")
    axes[0].set_xlim(0, 200)
    axes[0].legend()

    axes[1].plot(freqs_raw, mag_raw, linewidth=0.8, label="Raw", alpha=0.8)
    axes[1].plot(freqs_raw, mag_filtered, linewidth=0.8, label="Filtered", alpha=0.8)
    axes[1].set_title("Full spectrum")
    axes[1].set_xlabel("Frequency (Hz)")
    axes[1].set_ylabel("Magnitude")
    axes[1].set_xlim(0, actual_rate / 2)
    axes[1].legend()

    plt.tight_layout()
    plt.show()


if __name__ == '__main__':
    main(port=sys.argv[1] if len(sys.argv) > 1 else None)
