from typing import Any
from functools import partial

import sys
import signal
import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque

from audio.common import DCOffset, RateEstimator, BAUD_RATE, MicReader, wait_for_port, exp_mov_avg
from audio.filters import make_notch_filter, make_highpass_filter, make_pipeline

MAX_SIGNAL = 2100000
MIN_SIGNAL = -MAX_SIGNAL


def setup_plot(min_signal: int, max_signal: int, titles: list[str], buffers: list[deque[int]]) -> tuple[Any, Any, Any]:
    fig, axes = plt.subplots(3, 1, figsize=(10, 8))
    lines = []
    for ax, buf, title in zip(axes, buffers, titles):
        line, = ax.plot(buf)
        ax.set_ylim(min_signal, max_signal)
        ax.set_title(title)
        ax.set_ylabel("Sample Value")
        lines.append(line)
    axes[-1].set_xlabel("Sample")
    plt.tight_layout()
    return fig, axes, lines


def update(
    _frame: Any,
    reader: MicReader,
    lines: list[Any],
    axes: Any,
    buffers: list[deque[int]],
    dc_offset: DCOffset,
    rate_estimator: RateEstimator,
    windows: list[int],
    pipeline: Any,
) -> Any:
    for val in reader.read():
        dc_offset.value = exp_mov_avg(dc_offset.value, val)
        clean = pipeline(val - dc_offset.value)
        rate_estimator.add()
        for buf in buffers:
            buf.append(int(clean))
    if rate_estimator.count > 100:
        rate = rate_estimator.rate()
        long_buf = list(buffers[-1])
        rms = (sum(x ** 2 for x in long_buf) / len(long_buf)) ** 0.5
        for ax, window in zip(axes, windows):
            ax.set_title(f"{window} samples — {window / rate * 1000:.1f} ms @ {rate:.1f} Hz — RMS {rms:.0f}")
    for line, buf, ax in zip(lines, buffers, axes):
        line.set_ydata(buf)
        peak = max(abs(max(buf)), abs(min(buf)))
        if peak > 0:
            ax.set_ylim(-peak * 1.1, peak * 1.1)
    return lines


def main(
    port: str | None = None,
    baud: int = BAUD_RATE,
    windows: list[int] = [100, 1000, 10000],
    titles: list[str] = ["Short (100 samples)", "Medium (1000 samples)", "Long (10000 samples)"],
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

    buffers: list[deque[int]] = [deque([0] * w, maxlen=w) for w in windows]
    dc_offset = DCOffset(value=0.0)
    rate_estimator = RateEstimator()
    reader = MicReader(ser)
    pipeline = make_pipeline(
        make_notch_filter(50.0, 16000.0),
        make_highpass_filter(30.0, 16000.0, order=4),
    )
    fig, axes, lines = setup_plot(MIN_SIGNAL, MAX_SIGNAL, titles, buffers)

    ani = animation.FuncAnimation(  # noqa: F841
        fig,
        partial(update, reader=reader, lines=lines, axes=axes, buffers=buffers, dc_offset=dc_offset, rate_estimator=rate_estimator, windows=windows, pipeline=pipeline),
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
