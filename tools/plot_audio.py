import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque

import sys
PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
BAUD = 115200

WINDOWS = [100, 1000, 10000]
TITLES = ["Short (100 samples)", "Medium (1000 samples)", "Long (10000 samples)"]

buffers = [deque([0] * w, maxlen=w) for w in WINDOWS]

ser = serial.Serial(PORT, BAUD, timeout=1)

fig, axes = plt.subplots(3, 1, figsize=(10, 8))
lines = []
for ax, buf, title in zip(axes, buffers, TITLES):
    line, = ax.plot(buf)
    ax.set_ylim(-2100000, 2100000)
    ax.set_title(title)
    ax.set_ylabel("Sample Value")
    lines.append(line)
axes[-1].set_xlabel("Sample")
plt.tight_layout()

def update(_frame):
    try:
        while ser.in_waiting:
            try:
                val = int(ser.readline().decode().strip())
                for buf in buffers:
                    buf.append(val)
            except ValueError:
                pass
    except serial.SerialException:
        if ani.event_source is not None:
            ani.event_source.stop()
        ser.close()
        axes[0].set_title("Device disconnected")
        plt.draw()
    for line, buf, ax in zip(lines, buffers, axes):
        line.set_ydata(buf)
        if max(buf) != min(buf):
            margin = (max(buf) - min(buf)) * 0.1
            ax.set_ylim(min(buf) - margin, max(buf) + margin)
    return lines

ani = animation.FuncAnimation(fig, update, interval=50, blit=True)
plt.show()
ser.close()
