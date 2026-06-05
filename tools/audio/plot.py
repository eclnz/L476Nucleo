from typing import Any
from functools import partial

import sys
import signal
import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque

from audio.common import DCOffset, SAMPLE_RATE, wait_for_port, exp_mov_avg

MAX_SIGNAL = 2100000
MIN_SIGNAL = -MAX_SIGNAL


def setup_plot(min_signal: int, max_signal: int, titles: list[str], buffers: list[deque[int]], windows: list[int], sample_rate: float) -> tuple[Any, Any, Any]:
    fig, axes = plt.subplots(3, 1, figsize=(10, 8))
    lines = []
    for ax, buf, title, window in zip(axes, buffers, titles, windows):
        line, = ax.plot(buf)
        ax.set_ylim(min_signal, max_signal)
        duration = window / sample_rate
        ax.set_title(f"{title} — {duration * 1000:.1f} ms @ {sample_rate:.1f} Hz")
        ax.set_ylabel("Sample Value")
        lines.append(line)
    axes[-1].set_xlabel("Sample")
    plt.tight_layout()
    return fig, axes, lines


def update(
    _frame: Any,
    ser: serial.Serial,
    lines: list[Any],
    axes: Any,
    buffers: list[deque[int]],
    dc_offset: DCOffset,
) -> Any:
    while ser.in_waiting:
        try:
            val = int(ser.readline().decode().strip())
            dc_offset.value = exp_mov_avg(dc_offset.value, val)
            for buf in buffers:
                buf.append(int(val - dc_offset.value))
        except ValueError:
            pass
    for line, buf, ax in zip(lines, buffers, axes):
        line.set_ydata(buf)
        peak = max(abs(max(buf)), abs(min(buf)))
        if peak > 0:
            ax.set_ylim(-peak * 1.1, peak * 1.1)
    return lines


def main(
    port: str | None = None,
    baud: int = 115200,
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
    print(f"Sample rate: {SAMPLE_RATE} Hz")
    fig, axes, lines = setup_plot(MIN_SIGNAL, MAX_SIGNAL, titles, buffers, windows, SAMPLE_RATE)

    _ani = animation.FuncAnimation(
        fig,
        partial(update, ser=ser, lines=lines, axes=axes, buffers=buffers, dc_offset=dc_offset),
        interval=50,
        blit=False,
    )

    signal.signal(signal.SIGINT, lambda *_: plt.close())  # close plot on Ctrl+C so finally block can clean up
    try:
        plt.show()
    finally:
        ser.close()
        plt.close()


if __name__ == "__main__":
    main(port=sys.argv[1] if len(sys.argv) > 1 else None)
