"""
Conductor Audio Hardware I/O & File Processing Engine.
Manages duplex hardware audio streams (2 Inputs -> 1 Output) and offline test audio file processing.
"""

import io
import time
import threading
from typing import List, Dict, Any, Optional, Tuple
import numpy as np
import soundfile as sf

try:
    import sounddevice as sd
except Exception as e:
    sd = None

from .config import config
from .dsp_engine import CrossfadeDSPEngine
from .asr_engine import SpeechIngestionEngine


def get_available_devices() -> List[Dict[str, Any]]:
    """Returns a list of all audio I/O devices recognized by the system."""
def get_available_devices() -> Dict[str, Any]:
    """Returns detected system audio devices and default I/O indices."""
    if sd is None:
        return {"devices": [], "default_input": None, "default_output": None}

    device_list = []
    default_input = None
    default_output = None
    try:
        defaults = sd.default.device
        default_input = defaults[0] if defaults and len(defaults) > 0 else None
        default_output = defaults[1] if defaults and len(defaults) > 1 else None
        devices = sd.query_devices()
        for idx, dev in enumerate(devices):
            device_list.append({
                "id": idx,
                "name": dev.get("name", f"Device {idx}"),
                "inputs": dev.get("max_input_channels", 0),
                "outputs": dev.get("max_output_channels", 0),
                "max_input_channels": dev.get("max_input_channels", 0),
                "max_output_channels": dev.get("max_output_channels", 0),
                "default_samplerate": dev.get("default_samplerate", 48000),
                "hostapi": dev.get("hostapi", 0),
                "is_default_input": (idx == default_input),
                "is_default_output": (idx == default_output)
            })
    except Exception as e:
        print(f"[AudioIO] Device query error: {e}")

    return {
        "devices": device_list,
        "default_input": default_input,
        "default_output": default_output
    }


class Tone3000HardwareDuplexStream:
    """
    Manages dedicated low-latency hardware duplex audio streaming.
    Routes:
      - Input: Voice Mic channel -> SpeechIngestionEngine (optional).
      - Input: Guitar Line-In channel -> CrossfadeDSPEngine.
      - Output: Stereo Speakers/Headphones -> Processed Guitar (zero mic bleed).
    """

    def __init__(
        self,
        dsp_engine: CrossfadeDSPEngine,
        asr_engine: Optional[SpeechIngestionEngine] = None,
        device_id: Optional[Any] = None,
        input_device_id: Optional[Any] = None,
        output_device_id: Optional[Any] = None,
        mic_ch: int = 0,
        guitar_ch: int = 1,
        mic_enabled: bool = True,
        sample_rate: int = 48000,
        buffer_size: int = 512
    ):
        self.dsp_engine = dsp_engine
        self.asr_engine = asr_engine
        self.sample_rate = sample_rate
        self.buffer_size = buffer_size

        if isinstance(device_id, (tuple, list)):
            self.input_device_id = device_id[0]
            self.output_device_id = device_id[1]
        else:
            self.input_device_id = input_device_id if input_device_id is not None else device_id
            self.output_device_id = output_device_id if output_device_id is not None else device_id

        self.mic_ch = mic_ch
        self.guitar_ch = guitar_ch
        self.mic_enabled = mic_enabled

        self.stream: Optional[Any] = None
        self.is_running: bool = False
        self._lock = threading.Lock()

        self._in_dev_name: str = "Default Input"
        self._out_dev_name: str = "Default Output"
        self._in_channels: int = 0
        self._out_channels: int = 0

        # Telemetry & Tuner State
        self._tuner_buf_len = 2048
        self._tuner_buf = np.zeros(self._tuner_buf_len, dtype=np.float32)
        self._clip_in_until: float = 0.0
        self._clip_out_until: float = 0.0
        self._latest_telemetry: Dict[str, Any] = {
            "input_peak_db": -60.0,
            "input_rms_db": -60.0,
            "output_peak_db": -60.0,
            "output_rms_db": -60.0,
            "input_clipping": False,
            "output_clipping": False,
            "tuner": {
                "detected": False,
                "note": "--",
                "octave": 0,
                "freq_hz": 0.0,
                "cents": 0.0,
                "in_tune": False
            }
        }

    def _detect_pitch(self, signal: np.ndarray) -> Dict[str, Any]:
        """Calculates fundamental guitar frequency and chromatic tuning offset using autocorrelation."""
        rms = np.sqrt(np.mean(signal ** 2))
        if rms < 0.006:
            return {"detected": False, "note": "--", "octave": 0, "freq_hz": 0.0, "cents": 0.0, "in_tune": False}

        sig = signal - np.mean(signal)
        min_lag = int(self.sample_rate / 1000)  # ~1000 Hz
        max_lag = int(self.sample_rate / 65)    # ~65 Hz (Drop C/D)
        if len(sig) < max_lag:
            return {"detected": False, "note": "--", "octave": 0, "freq_hz": 0.0, "cents": 0.0, "in_tune": False}

        corr = np.correlate(sig, sig, mode="full")[len(sig) - 1:]
        search_region = corr[min_lag:max_lag]
        if len(search_region) == 0:
            return {"detected": False, "note": "--", "octave": 0, "freq_hz": 0.0, "cents": 0.0, "in_tune": False}

        peak_idx = int(np.argmax(search_region)) + min_lag
        peak_val = corr[peak_idx]

        if corr[0] > 0 and (peak_val / corr[0]) > 0.38:
            if 0 < peak_idx < len(corr) - 1:
                alpha, beta, gamma = float(corr[peak_idx - 1]), float(corr[peak_idx]), float(corr[peak_idx + 1])
                denom = 2 * (2 * beta - alpha - gamma)
                delta = (alpha - gamma) / denom if abs(denom) > 1e-9 else 0.0
                actual_lag = peak_idx + delta
            else:
                actual_lag = float(peak_idx)

            freq = float(self.sample_rate / actual_lag)
            if 60.0 <= freq <= 1100.0:
                note_names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
                midi_num = 12.0 * np.log2(freq / 440.0) + 69.0
                closest_midi = int(round(midi_num))
                note = note_names[closest_midi % 12]
                octave = (closest_midi // 12) - 1
                cents = round(float(midi_num - closest_midi) * 100.0, 1)
                in_tune = abs(cents) <= 3.0
                return {
                    "detected": True,
                    "note": note,
                    "octave": octave,
                    "freq_hz": round(freq, 1),
                    "cents": cents,
                    "in_tune": in_tune
                }

        return {"detected": False, "note": "--", "octave": 0, "freq_hz": 0.0, "cents": 0.0, "in_tune": False}

    def _audio_callback(self, indata, outdata, frames, time_info, status):
        """Low-latency audio callback running on audio thread."""
        num_in_channels = indata.shape[1] if indata.ndim > 1 else 1

        # 1. Voice mic processing (only if mic is enabled)
        if self.mic_enabled and self.asr_engine and num_in_channels > self.mic_ch:
            mic_data = indata[:, self.mic_ch]
            self.asr_engine.process_pcm_chunk(mic_data)

        # 2. Guitar line-in processing
        if num_in_channels > self.guitar_ch:
            guitar_in = indata[:, self.guitar_ch]
        elif num_in_channels > 0:
            guitar_in = indata[:, 0]
        else:
            guitar_in = np.zeros(frames, dtype=np.float32)

        # Expand to stereo for guitar processing
        guitar_stereo = np.column_stack([guitar_in, guitar_in])
        processed_guitar = self.dsp_engine.process(guitar_stereo, sample_rate=self.sample_rate)

        # 3. Output routing
        if outdata.shape[1] >= 2:
            outdata[:] = processed_guitar[:, :outdata.shape[1]]
        elif outdata.shape[1] == 1:
            outdata[:, 0] = processed_guitar.mean(axis=1)

        # 4. Telemetry: dBFS Metering, Clipping Detection & Live Tuner Buffer
        in_peak = float(np.max(np.abs(guitar_in))) if len(guitar_in) > 0 else 0.0
        in_rms = float(np.sqrt(np.mean(guitar_in ** 2))) if len(guitar_in) > 0 else 0.0
        out_peak = float(np.max(np.abs(processed_guitar))) if len(processed_guitar) > 0 else 0.0
        out_rms = float(np.sqrt(np.mean(processed_guitar ** 2))) if len(processed_guitar) > 0 else 0.0

        now = time.time()
        if in_peak >= 0.98:
            self._clip_in_until = now + 1.5
        if out_peak >= 0.98:
            self._clip_out_until = now + 1.5

        # Update tuner ring buffer
        if frames <= self._tuner_buf_len:
            self._tuner_buf = np.roll(self._tuner_buf, -frames)
            self._tuner_buf[-frames:] = guitar_in

        # Compute dBFS values
        in_peak_db = round(float(20.0 * np.log10(max(in_peak, 1e-4))), 1)
        in_rms_db = round(float(20.0 * np.log10(max(in_rms, 1e-4))), 1)
        out_peak_db = round(float(20.0 * np.log10(max(out_peak, 1e-4))), 1)
        out_rms_db = round(float(20.0 * np.log10(max(out_rms, 1e-4))), 1)

        tuner_res = self._detect_pitch(self._tuner_buf)

        self._latest_telemetry = {
            "input_peak_db": in_peak_db,
            "input_rms_db": in_rms_db,
            "output_peak_db": out_peak_db,
            "output_rms_db": out_rms_db,
            "input_clipping": (now < self._clip_in_until),
            "output_clipping": (now < self._clip_out_until),
            "tuner": tuner_res
        }

    def get_telemetry(self) -> Dict[str, Any]:
        """Thread-safe getter for real-time meters and live tuner readout."""
        return dict(self._latest_telemetry)

    def start(self) -> bool:
        """Starts real-time hardware stream."""
        if sd is None:
            print("[AudioIO] sounddevice not available. Running in offline/server mode.")
            return False

        with self._lock:
            if self.is_running:
                return True

            try:
                default_devs = sd.default.device
                in_dev = self.input_device_id if self.input_device_id is not None else (default_devs[0] if default_devs else None)
                out_dev = self.output_device_id if self.output_device_id is not None else (default_devs[1] if default_devs else None)

                if in_dev is None or out_dev is None:
                    print("[AudioIO] Notice: No audio input or output device detected.")
                    return False

                in_info = sd.query_devices(in_dev)
                out_info = sd.query_devices(out_dev)

                max_in = in_info.get("max_input_channels", 0)
                max_out = out_info.get("max_output_channels", 0)

                self._in_dev_name = in_info.get("name", f"Device {in_dev}")
                self._out_dev_name = out_info.get("name", f"Device {out_dev}")

                if max_in <= 0 or max_out <= 0:
                    print(f"[AudioIO] Notice: Device ({self._in_dev_name}) lacks I/O channels (in={max_in}, out={max_out}). Running in offline/web mode.")
                    return False

                target_in_ch = max(self.mic_ch, self.guitar_ch) + 1
                in_channels = min(max_in, target_in_ch)
                out_channels = min(max_out, 2)

                self._in_channels = in_channels
                self._out_channels = out_channels

                self.stream = sd.Stream(
                    samplerate=self.sample_rate,
                    blocksize=self.buffer_size,
                    device=(in_dev, out_dev),
                    channels=(in_channels, out_channels),
                    dtype="float32",
                    callback=self._audio_callback
                )
                self.stream.start()
                self.is_running = True
                print(f"[AudioIO] Real-time audio stream active: [{self._in_dev_name} ({in_channels} In)] -> [{self._out_dev_name} ({out_channels} Out)] at {self.sample_rate}Hz.")
                return True
            except Exception as e:
                print(f"[AudioIO] Hardware stream offline: {e}")
                self.stream = None
                self.is_running = False
                return False

    def stop(self) -> None:
        """Stops hardware stream cleanly."""
        with self._lock:
            if self.stream:
                try:
                    self.stream.stop()
                    self.stream.close()
                except Exception:
                    pass
                self.stream = None
            self.is_running = False

    def reconfigure(
        self,
        input_device_id: Optional[Any] = None,
        output_device_id: Optional[Any] = None,
        mic_ch: int = 0,
        guitar_ch: int = 1,
        mic_enabled: bool = True
    ) -> bool:
        """Updates device routing and cleanly restarts the hardware stream."""
        self.stop()
        self.input_device_id = input_device_id
        self.output_device_id = output_device_id
        self.mic_ch = mic_ch
        self.guitar_ch = guitar_ch
        self.mic_enabled = mic_enabled
        return self.start()

    def get_status(self) -> Dict[str, Any]:
        """Returns active hardware connection and routing status."""
        return {
            "is_running": self.is_running,
            "input_device_id": self.input_device_id,
            "output_device_id": self.output_device_id,
            "input_device_name": self._in_dev_name if self.is_running else None,
            "output_device_name": self._out_dev_name if self.is_running else None,
            "in_channels": self._in_channels,
            "out_channels": self._out_channels,
            "mic_channel": self.mic_ch,
            "guitar_channel": self.guitar_ch,
            "mic_enabled": self.mic_enabled
        }


def process_audio_file(
    file_bytes: bytes,
    dsp_engine: CrossfadeDSPEngine,
    sample_rate: int = 48000
) -> Tuple[bytes, float]:
    """
    Processes an in-memory audio file (e.g., test guitar track) through the active DSP engine.
    Returns (wav_bytes, duration_seconds).
    """
    with io.BytesIO(file_bytes) as in_buf:
        audio_data, file_sr = sf.read(in_buf, dtype="float32")

    # If mono, convert to stereo
    if audio_data.ndim == 1:
        audio_data = np.column_stack([audio_data, audio_data])

    duration = len(audio_data) / float(file_sr)

    # Process through Pedalboard DSP engine
    processed = dsp_engine.process(audio_data, sample_rate=file_sr)

    with io.BytesIO() as out_buf:
        sf.write(out_buf, processed, file_sr, format="WAV", subtype="FLOAT")
        out_bytes = out_buf.getvalue()

    return out_bytes, duration
