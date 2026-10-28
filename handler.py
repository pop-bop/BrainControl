import asyncio
import threading
import time
from collections import deque

import numpy as np
from bleak import BleakClient, BleakScanner
from scipy import signal

DEVICE_PREFIX = "NPG"
DATA_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
CONTROL_CHAR_UUID = "0000ff01-0000-1000-8000-00805f9b34fb"
SAMPLE_LEN = 7

FS = 500.0
MAINS_HZ = 50.0

LEFT_CH = 0
RIGHT_CH = 1
ARM_CHANNELS = (LEFT_CH, RIGHT_CH)
ARM_NAMES = ("left", "right")

LEFT_LIMIT = 0.06
RIGHT_LIMIT = 0.04
ARM_LIMITS = {
    "left": LEFT_LIMIT,
    "right": RIGHT_LIMIT,
}
OFF_RATIO = 0.7

SETTLE_SECONDS = 1.0
CALIB_SECONDS = 3.0

EMG_LOW = 20.0
EMG_HIGH = 200.0
ENV_TAU_S = 0.05
BASELINE_TAU_S = 30.0

CLENCH_MIN_ON_S = 0.04
CLENCH_OFF_HANG_S = 0.10
CLENCH_HOLD_S = 0.15

HIST_LEN = 1000


class StreamFilter:
    def __init__(self, sos):
        self.sos = sos
        self.zi = None

    def __call__(self, x):
        if self.zi is None:
            self.zi = signal.sosfilt_zi(self.sos) * x[0]
        y, self.zi = signal.sosfilt(self.sos, x, zi=self.zi)
        return y


def make_chain(sos_list):
    return StreamFilter(np.vstack(sos_list))


class ArmChannelTracker:
    """Tracks a single arm channel's normalized EMG envelope against its limit."""

    def __init__(self, fs: float, limit: float):
        self.fs = fs
        self.limit = limit
        self.base_alpha = 1.0 - np.exp(-1.0 / (fs * BASELINE_TAU_S))
        self.on_need = max(1, int(CLENCH_MIN_ON_S * fs))
        self.off_need = max(1, int(CLENCH_OFF_HANG_S * fs))

        self.calib_env = []
        self.mu = None
        self.on_th = limit
        self.off_th = limit * OFF_RATIO

        self.active = False
        self.on_count = 0
        self.off_count = 0
        self.last_active_t = -1e9
        self.hist = deque(maxlen=HIST_LEN)

    def step(self, v: float, t: float, calibrated: bool):
        self.hist.append(v)
        if t < SETTLE_SECONDS:
            return
        if not calibrated:
            self.calib_env.append(v)
            return
        if self.mu is None:
            arr = np.array(self.calib_env, dtype=float)
            self.mu = float(np.median(arr)) if len(arr) else 0.0

        # Baseline-subtracted normalized envelope compared against the limit
        delta = max(0.0, v - self.mu)
        self.on_th = self.limit
        self.off_th = self.limit * OFF_RATIO

        if self.active:
            self.off_count = self.off_count + 1 if delta < self.off_th else 0
            if self.off_count >= self.off_need:
                self.active = False
                self.on_count = 0
        else:
            self.on_count = self.on_count + 1 if delta >= self.on_th else 0
            if self.on_count >= self.on_need:
                self.active = True
                self.off_count = 0

        if self.active:
            self.last_active_t = t
        elif delta < self.off_th:
            self.mu += self.base_alpha * (v - self.mu)

    def is_clenched(self, t: float) -> bool:
        if self.mu is None:
            return False
        return self.active or (t - self.last_active_t < CLENCH_HOLD_S)


class StateDetector:
    def __init__(self):
        nyq = FS / 2.0
        n = len(ARM_CHANNELS)

        dc = signal.butter(2, 0.5 / nyq, btype="highpass", output="sos")
        notches = []
        for harm in (1, 2, 3, 4):
            freq = MAINS_HZ * harm
            if freq < nyq:
                b, a = signal.iirnotch(freq, Q=30, fs=FS)
                notches.append(signal.tf2sos(b, a))
        emg_bp = signal.butter(
            4, [EMG_LOW / nyq, EMG_HIGH / nyq], btype="bandpass", output="sos"
        )
        self.emg_filters = [make_chain([dc, *notches, emg_bp]) for _ in range(n)]

        al = 1.0 - np.exp(-1.0 / (FS * ENV_TAU_S))
        self.env_b = [al]
        self.env_a = [1.0, -(1.0 - al)]
        self.env_zi = [np.zeros(1) for _ in range(n)]

        self.trackers = {
            name: ArmChannelTracker(FS, ARM_LIMITS[name]) for name in ARM_NAMES
        }

        self.lock = threading.Lock()
        self.t = 0.0
        self._scale = None
        self._calib_logged = False
        self._state = "none"

    def pull_state(self) -> str:
        with self.lock:
            return self._state

    @property
    def calibrated(self) -> bool:
        return self.t >= CALIB_SECONDS

    def snapshot(self):
        with self.lock:
            left = self.trackers["left"]
            right = self.trackers["right"]
            return {
                "left_env": np.array(left.hist),
                "right_env": np.array(right.hist),
                "left_on": left.on_th,
                "left_off": left.off_th,
                "right_on": right.on_th,
                "right_off": right.off_th,
                "state": self._state,
                "calibrated": self.calibrated,
            }

    def update(self, samples):
        if not samples:
            return
        raw = np.array(
            [[s[1 + c] for c in ARM_CHANNELS] for s in samples], dtype=float
        )

        with self.lock:
            if self._scale is None:
                # Auto-detect 12-bit (NPG Lite ESP32-C6, 0..4095 -> half-scale 2048)
                # vs 16-bit signed ADC (-> half-scale 32768).
                peak = float(np.max(np.abs(raw)))
                self._scale = 32768.0 if peak > 4096.0 else 2048.0

            norm_raw = raw / self._scale
            emg_c = np.array(
                [f(norm_raw[:, i]) for i, f in enumerate(self.emg_filters)]
            )

            envs = []
            for i in range(len(ARM_CHANNELS)):
                pw, self.env_zi[i] = signal.lfilter(
                    self.env_b, self.env_a, emg_c[i] ** 2, zi=self.env_zi[i]
                )
                envs.append(np.sqrt(np.clip(pw, 0.0, None)))

            left_env, right_env = envs[0], envs[1]
            left_tr = self.trackers["left"]
            right_tr = self.trackers["right"]

            for lv, rv in zip(left_env, right_env):
                self.t += 1.0 / FS
                cal = self.calibrated
                left_tr.step(float(lv), self.t, cal)
                right_tr.step(float(rv), self.t, cal)

            if (
                not self._calib_logged
                and left_tr.mu is not None
                and right_tr.mu is not None
            ):
                self._calib_logged = True
                print(
                    f"[calib] left: base={left_tr.mu:.4f} limit={left_tr.on_th:.2f} | "
                    f"right: base={right_tr.mu:.4f} limit={right_tr.on_th:.2f}"
                )

            self._decide()

    def _decide(self):
        if not self.calibrated:
            self._state = "none"
            return

        left_on = self.trackers["left"].is_clenched(self.t)
        right_on = self.trackers["right"].is_clenched(self.t)

        if left_on and right_on:
            self._state = "both"
        elif left_on:
            self._state = "left"
        elif right_on:
            self._state = "right"
        else:
            self._state = "none"


_default_detector = StateDetector()
_bg_thread = None
_bg_lock = threading.Lock()
_ready_event = threading.Event()
_stream_error: BaseException | None = None


def parse_packet(data: bytearray):
    samples = []
    for i in range(0, len(data) - SAMPLE_LEN + 1, SAMPLE_LEN):
        chunk = data[i : i + SAMPLE_LEN]
        counter = chunk[0]
        chans = [
            int.from_bytes(chunk[1 + 2 * c : 3 + 2 * c], byteorder="big", signed=True)
            for c in range(3)
        ]
        samples.append((counter, *chans))
    return samples


async def find_device():
    print("Scanning for NPG Lite...")
    device = await BleakScanner.find_device_by_filter(
        lambda d, ad: (
            (d.name or "").upper().startswith(DEVICE_PREFIX)
            or (ad.local_name or "").upper().startswith(DEVICE_PREFIX)
        ),
        timeout=10.0,
    )
    if device is None:
        raise RuntimeError(
            "No NPG device found. Make sure it is powered on, in BLE mode, "
            "and not already connected to another app/tab (e.g. Chords-Web)."
        )
    print(f"Found {device.name or device.address} [{device.address}]")
    return device


async def stream_device(detector: StateDetector = _default_detector):
    global _stream_error
    try:
        device = await find_device()
        queue: asyncio.Queue = asyncio.Queue()

        def on_notify(_sender, data: bytearray):
            queue.put_nowait(parse_packet(data))

        async with BleakClient(device) as client:
            print(
                f"Connected. Calibrating for {CALIB_SECONDS:.0f}s - relax both arms..."
            )
            await client.start_notify(DATA_CHAR_UUID, on_notify)
            await client.write_gatt_char(CONTROL_CHAR_UUID, b"START", response=True)
            try:
                while True:
                    try:
                        samples = await asyncio.wait_for(queue.get(), timeout=2.0)
                    except asyncio.TimeoutError:
                        continue
                    detector.update(samples)
                    if detector.calibrated and not _ready_event.is_set():
                        _ready_event.set()
            finally:
                await client.stop_notify(DATA_CHAR_UUID)
    except BaseException as exc:
        _stream_error = exc
        _ready_event.set()
        raise


def ensure_streaming(wait_for_calibration: bool = True):
    global _bg_thread, _stream_error
    with _bg_lock:
        if _bg_thread is None or not _bg_thread.is_alive():
            _stream_error = None
            _ready_event.clear()
            _bg_thread = threading.Thread(
                target=lambda: asyncio.run(stream_device(_default_detector)),
                daemon=True,
            )
            _bg_thread.start()

    if wait_for_calibration:
        while not _ready_event.wait(timeout=0.1):
            if _bg_thread is not None and not _bg_thread.is_alive():
                break
        if _stream_error is not None:
            raise _stream_error


def pull_state() -> str:
    """Returns the current arm clench state: 'left', 'right', 'both', or 'none'."""
    if _stream_error is not None:
        raise _stream_error
    return _default_detector.pull_state()


if __name__ == "__main__":
    try:
        ensure_streaming(wait_for_calibration=True)
        while True:
            print(pull_state())
            time.sleep(0.05)
    except KeyboardInterrupt:
        pass
