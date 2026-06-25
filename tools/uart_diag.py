#!/usr/bin/env python3
"""
DFSPM / SD-card rate diagnostics listener.

Listens for "!D,..." lines emitted by the firmware every 1 s and computes
per-second rates for each instrumented stage.

Usage:
    python3 uart_diag.py /dev/ttyACM0        # default 250000 baud
    python3 uart_diag.py /dev/ttyACM0 115200

Packet format from firmware:
    !D,<tick_ms>,<dma_cbs>,<samples_pushed>,<sd_bytes>,<missed_bufs>,<ring_bytes>\r\n

All counters except ring_bytes are cumulative and never reset.
ring_bytes is the instantaneous ring-buffer fill level.
"""

import sys
import time
import serial

# Expected values at 40 kHz with AUDIO_BUF_HALF=512 samples
EXPECTED_DMA_HZ      = 40_000 / 512 * 2   # half + full callbacks = 156.25 Hz
EXPECTED_SAMPLE_RATE = 40_000              # samples/s pushed to ring
EXPECTED_SD_BPS      = 40_000 * 2         # bytes/s (int16 = 2 bytes/sample)
RING_CAPACITY        = 512 * 16 * 2       # RINGBUF_SIZE entries * sizeof(int16) = 16384 bytes

WARN_THRESHOLD = 0.05   # warn if rate deviates >5% from expected


def _status(rate: float, expected: float) -> str:
    if expected == 0:
        return ""
    deviation = abs(rate - expected) / expected
    return "OK" if deviation <= WARN_THRESHOLD else f"WARN ({deviation*100:.1f}% off)"


def _bar(fill: int, capacity: int, width: int = 20) -> str:
    filled = int(width * fill / capacity) if capacity else 0
    return "[" + "#" * filled + "." * (width - filled) + "]"


def run(port: str, baud: int) -> None:
    print(f"Opening {port} at {baud} baud …")
    ser = serial.Serial(port, baud, timeout=2.0)
    print("Waiting for first diagnostic packet …\n")

    prev: dict | None = None

    while True:
        raw = ser.readline()
        if not raw:
            continue

        try:
            line = raw.decode("ascii", errors="replace").strip()
        except Exception:
            continue

        if not line.startswith("!D,"):
            # Forward non-diagnostic lines (boot messages, etc.) to stdout
            print(f"  [{line}]")
            continue

        parts = line[3:].split(",")
        if len(parts) != 6:
            continue

        try:
            tick_ms, dma_cbs, samp_pushed, sd_bytes, missed, ring_bytes = (
                int(p) for p in parts
            )
        except ValueError:
            continue

        now = {
            "tick_ms":     tick_ms,
            "dma_cbs":     dma_cbs,
            "samp_pushed": samp_pushed,
            "sd_bytes":    sd_bytes,
            "missed":      missed,
            "ring_bytes":  ring_bytes,
        }

        if prev is None:
            prev = now
            print("First packet received — waiting for second to compute rates …")
            continue

        dt_s = (now["tick_ms"] - prev["tick_ms"]) / 1000.0
        if dt_s <= 0:
            prev = now
            continue

        dma_rate    = (now["dma_cbs"]     - prev["dma_cbs"])     / dt_s
        samp_rate   = (now["samp_pushed"] - prev["samp_pushed"]) / dt_s
        sd_rate     = (now["sd_bytes"]    - prev["sd_bytes"])     / dt_s
        missed_rate = (now["missed"]      - prev["missed"])       / dt_s
        ring_fill   = now["ring_bytes"]

        uptime_s = now["tick_ms"] / 1000.0

        header = (
            f"\n{'='*62}\n"
            f"  DFSPM / SD Diagnostics    uptime: {uptime_s:.1f} s\n"
            f"{'='*62}"
        )
        print(header)

        rows = [
            ("DFSDM DMA callbacks",
             f"{dma_rate:7.2f} Hz",
             f"{EXPECTED_DMA_HZ:.2f} Hz",
             _status(dma_rate, EXPECTED_DMA_HZ)),

            ("Sample throughput",
             f"{samp_rate:7.0f} smp/s",
             f"{EXPECTED_SAMPLE_RATE} smp/s",
             _status(samp_rate, EXPECTED_SAMPLE_RATE)),

            ("SD write rate",
             f"{sd_rate/1000:7.2f} KB/s",
             f"{EXPECTED_SD_BPS/1000:.1f} KB/s",
             _status(sd_rate, EXPECTED_SD_BPS)),

            ("Missed (ring drops)",
             f"{missed_rate:7.2f} /s",
             "0.00 /s",
             "OK" if missed_rate == 0 else f"WARN ({missed_rate:.1f}/s)"),
        ]

        col_w = [30, 16, 14, 8]
        fmt   = f"  {{:<{col_w[0]}}}{{:>{col_w[1]}}}  {{:>{col_w[2]}}}  {{}}"
        print(fmt.format("Metric", "Measured", "Expected", "Status"))
        print("  " + "-" * (sum(col_w) + 6))
        for r in rows:
            print(fmt.format(*r))

        pct = ring_fill / RING_CAPACITY * 100
        bar = _bar(ring_fill, RING_CAPACITY)
        print(f"\n  Ring buffer: {bar} {ring_fill}/{RING_CAPACITY} bytes ({pct:.1f}%)")
        print(f"  Cumulative misses: {now['missed']}")

        prev = now


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    port = sys.argv[1]
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else 250_000

    try:
        run(port, baud)
    except KeyboardInterrupt:
        print("\nStopped.")
    except serial.SerialException as exc:
        print(f"Serial error: {exc}", file=sys.stderr)
        sys.exit(1)
