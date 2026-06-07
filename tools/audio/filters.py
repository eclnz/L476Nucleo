from typing import Callable

from scipy.signal import iirnotch, butter


def make_notch_filter(freq: float, fs: float, Q: float = 30.0) -> Callable[[float], float]:
    b, a = iirnotch(freq, Q, fs)
    state = [0.0, 0.0]
    def apply(x: float) -> float:
        y = b[0] * x + state[0]
        state[0] = b[1] * x - a[1] * y + state[1]
        state[1] = b[2] * x - a[2] * y
        return y
    return apply


def make_highpass_filter(cutoff: float, fs: float, order: int = 2) -> Callable[[float], float]:
    sos = butter(order, cutoff, btype='high', fs=fs, output='sos')
    n_sections = sos.shape[0]
    state = [[0.0, 0.0] for _ in range(n_sections)]
    def apply(x: float) -> float:
        for i, (s, sec) in enumerate(zip(state, sos)):
            b0, b1, b2, _, a1, a2 = sec
            y = b0 * x + s[0]
            s[0] = b1 * x - a1 * y + s[1]
            s[1] = b2 * x - a2 * y
            state[i] = s
            x = y
        return x
    return apply


def make_pipeline(*filters: Callable[[float], float]) -> Callable[[float], float]:
    def apply(x: float) -> float:
        for f in filters:
            x = f(x)
        return x
    return apply
