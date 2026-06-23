import sys
import numpy as np
import sounddevice as sd
from scipy.signal import sosfilt, butter, iirnotch

SAMPLE_RATE = 40000


def load(path: str) -> np.ndarray:
    data = np.fromfile(path, dtype="<i2").astype(np.float32)
    if data.size == 0:
        raise ValueError(f"{path} is empty")
    return data / 32768.0


def process(samples: np.ndarray, fs: int) -> np.ndarray:
    from scipy.signal import lfilter
    sos_hp: np.ndarray = butter(4, 30.0, btype="high", fs=fs, output="sos")  # type: ignore[assignment]
    b_n: np.ndarray
    a_n: np.ndarray
    b_n, a_n = iirnotch(50.0, 30.0, fs)
    filtered: np.ndarray = sosfilt(sos_hp, samples)  # type: ignore[assignment]
    filtered = lfilter(b_n, a_n, filtered)  # type: ignore[assignment]
    return filtered.astype(np.float32)


def main(path: str | None = None, rate: int = SAMPLE_RATE) -> None:
    if path is None:
        print("Usage: play_bin <file.bin> [sample_rate]")
        sys.exit(1)

    samples = load(path)
    duration = len(samples) / rate
    print(f"{path}: {len(samples)} samples, {duration:.2f}s at {rate} Hz")

    samples = process(samples, rate)
    print("Playing...")
    sd.play(samples, samplerate=rate)
    sd.wait()
    print("Done.")


if __name__ == "__main__":
    main(
        path=sys.argv[1] if len(sys.argv) > 1 else None,
        rate=int(sys.argv[2]) if len(sys.argv) > 2 else SAMPLE_RATE,
    )
