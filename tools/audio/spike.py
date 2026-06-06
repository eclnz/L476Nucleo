import sys
import signal
import serial
import numpy as np
import matplotlib.pyplot as plt
from collections import deque

from audio.common import DCOffset, wait_for_port, exp_mov_avg, BAUD_RATE

PRE  = 2000   # samples before spike
POST = 4000   # samples after spike
THRESHOLD = 10.0  # multiples of running RMS to trigger


def main(port: str | None = None, baud: int = BAUD_RATE) -> None:
    if port is None:
        port = wait_for_port()
        if port is None:
            print("No device found")
            return

    print(f"Connecting to {port}...")
    ser = serial.Serial(port, baud, timeout=1)
    print(f"Waiting for spike (threshold {THRESHOLD}x RMS)...")

    signal.signal(signal.SIGINT, lambda *_: ser.close())

    dc = DCOffset()
    pre_buf: deque[float] = deque(maxlen=PRE)
    rms_buf: deque[float] = deque(maxlen=1000)

    triggered = False
    post: list[float] = []

    try:
        while True:
            try:
                val = int(ser.readline().decode().strip())
            except ValueError:
                continue

            dc.value = exp_mov_avg(dc.value, val)
            clean = val - dc.value

            if not triggered:
                pre_buf.append(clean)
                rms_buf.append(clean ** 2)
                rms = (sum(rms_buf) / len(rms_buf)) ** 0.5
                if rms > 0 and abs(clean) > THRESHOLD * rms:
                    print(f"Spike detected: raw={val} clean={clean:.0f} ({abs(clean)/rms:.1f}x RMS)")
                    triggered = True
            else:
                post.append(clean)
                if len(post) >= POST:
                    break
    finally:
        ser.close()

    samples = np.array(list(pre_buf) + post)
    trigger_idx = len(pre_buf)

    fig, axes = plt.subplots(2, 1, figsize=(10, 6))

    axes[0].plot(samples)
    axes[0].axvline(trigger_idx, color='red', linestyle='--', label='trigger')
    axes[0].set_title(f"Spike profile — {PRE} pre / {POST} post samples")
    axes[0].set_ylabel("Amplitude")
    axes[0].legend()

    axes[1].specgram(samples, Fs=16000, cmap='inferno')
    axes[1].set_title("Spectrogram")
    axes[1].set_ylabel("Frequency (Hz)")
    axes[1].set_xlabel("Time (s)")

    plt.tight_layout()
    plt.show()


if __name__ == '__main__':
    main(port=sys.argv[1] if len(sys.argv) > 1 else None)
