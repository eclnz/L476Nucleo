from typing import Any
from functools import partial

import sys
import signal
import serial
import numpy as np
from scipy.ndimage import median_filter
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque

from audio.common import DCOffset, RateEstimator, BAUD_RATE, MicReader, wait_for_port, exp_mov_avg
from audio.filters import make_notch_filter, make_highpass_filter, make_pipeline

SPEC_FRAME = 4096
SPEC_HOP = 512
SPEC_BINS = SPEC_FRAME // 2 + 1  # 2049 bins (DC through Nyquist)
SAMPLE_BUF = 80000

MAX_FREQ = 2000
FREQ_TICKS = [50, 100, 200, 500, 1000, 2000]


def compute_spectrogram(buf: deque) -> np.ndarray:
    arr = np.array(buf, dtype=float)
    window = np.hanning(SPEC_FRAME)
    frames = []
    for i in range(0, len(arr) - SPEC_FRAME, SPEC_HOP):
        frame = arr[i:i + SPEC_FRAME] * window
        frames.append(np.abs(np.fft.rfft(frame)))
    if not frames:
        return np.zeros((SPEC_BINS - 1, 1))
    spec = np.array(frames).T[1:]  # drop DC bin
    db = 20 * np.log10(np.maximum(spec, 1e-6))
    db = median_filter(db, size=(1, 3))  # suppress broadband spike stripes along time axis
    return db


def setup_plot(n_frames: int) -> tuple[Any, Any, Any]:
    n_freq = SPEC_BINS - 1  # DC bin dropped
    fig, ax = plt.subplots(figsize=(10, 5))
    freq_edges = np.arange(n_freq + 1, dtype=float)
    time_edges = np.arange(n_frames + 1, dtype=float)
    spec_im = ax.pcolormesh(
        time_edges, freq_edges,
        np.full((n_freq, n_frames), -60.0),
        cmap="inferno", vmin=60, vmax=100, shading="flat",
    )
    ax.set_ylim(freq_edges[0], freq_edges[-1])
    ax.set_title("Live spectrogram")
    ax.set_xlabel("Time (frames)")
    ax.set_ylabel("Frequency (Hz)")
    plt.colorbar(spec_im, ax=ax, label="dB")
    plt.tight_layout()
    return fig, ax, spec_im


def update(
    _frame: Any,
    reader: MicReader,
    ax: Any,
    buf: deque,
    dc_offset: DCOffset,
    rate_estimator: RateEstimator,
    pipeline: Any,
    spec_im: Any,
) -> Any:
    for _seq, batch in reader.read():
        for val in batch:
            dc_offset.value = exp_mov_avg(dc_offset.value, float(val))
            clean = pipeline(float(val) - dc_offset.value)
            rate_estimator.add()
            buf.append(int(clean))

    if rate_estimator.count > 100:
        rate = rate_estimator.rate()
        duration = SAMPLE_BUF / rate
        ax.set_title(f"Live spectrogram — {duration:.1f} s window @ {rate:.1f} Hz")
        bin_hz = rate / SPEC_FRAME
        max_bin = MAX_FREQ / bin_hz
        ax.set_ylim(0, max_bin)
        ticks = [f / bin_hz for f in FREQ_TICKS if f <= MAX_FREQ]
        labels = [f"{f:g}" for f in FREQ_TICKS if f <= MAX_FREQ]
        if ticks:
            ax.set_yticks(ticks)
            ax.set_yticklabels(labels)

    spec = compute_spectrogram(buf)
    spec_im.set_array(spec.ravel())
    return [spec_im]


def main(port: str | None = None, baud: int = BAUD_RATE) -> None:
    if port is None:
        print("Waiting for ACM device...")
        port = wait_for_port()
        if port is None:
            print("Could not connect")
            return

    print(f"Connecting to {port}...")
    ser = serial.Serial(port, baud, timeout=1)
    print("Connected.")

    buf: deque[int] = deque([0] * SAMPLE_BUF, maxlen=SAMPLE_BUF)
    dc_offset = DCOffset(value=0.0)
    rate_estimator = RateEstimator()
    reader = MicReader(ser)
    pipeline = make_pipeline(
        make_notch_filter(50.0, 16000.0),
        make_highpass_filter(30.0, 16000.0, order=4),
    )
    n_frames = (SAMPLE_BUF - SPEC_FRAME + SPEC_HOP - 1) // SPEC_HOP
    fig, ax, spec_im = setup_plot(n_frames)

    ani = animation.FuncAnimation(  # noqa: F841
        fig,
        partial(update, reader=reader, ax=ax, buf=buf, dc_offset=dc_offset, rate_estimator=rate_estimator, pipeline=pipeline, spec_im=spec_im),
        interval=1,
        blit=False,
        cache_frame_data=False,
    )

    signal.signal(signal.SIGINT, lambda *_: plt.close())
    try:
        plt.show()
    finally:
        ser.close()
        plt.close()


if __name__ == "__main__":
    main(port=sys.argv[1] if len(sys.argv) > 1 else None)
