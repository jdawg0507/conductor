"""
Conductor Real-Time Audio DSP Engine.
Powered by Pedalboard with double-buffered equal-power crossfade for click-free live parameter tuning.
"""

import threading
import numpy as np
from typing import Optional, Dict, Any, List
import pedalboard
from pedalboard import (
    Pedalboard, Gain, Distortion, Reverb, Delay, Chorus, Phaser,
    PitchShift, HighpassFilter, LowpassFilter, PeakFilter,
    Compressor, NoiseGate, Bitcrush, Limiter, HighShelfFilter, LowShelfFilter, Clipping
)

from .schemas import DSPGraphConfig, DSPNodeConfig


NODE_PARAM_DEFAULTS: Dict[str, Dict[str, float]] = {
    "Amp": {
        "gain_db": 12.0,
        "drive_db": 12.0,
        "bass_db": 0.0,
        "mid_db": 0.0,
        "treble_db": 0.0,
        "presence_db": 0.0,
        "volume_db": 0.0
    },
    "Distortion": {
        "drive_db": 12.0,
        "tone": 0.55,
        "level_db": 0.0
    },
    "Delay": {
        "delay_seconds": 0.35,
        "feedback": 0.45,
        "mix": 0.35
    },
    "Reverb": {
        "room_size": 0.65,
        "damping": 0.3,
        "wet_level": 0.35,
        "dry_level": 0.85,
        "width": 1.0
    },
    "Chorus": {
        "rate_hz": 1.2,
        "depth": 0.55,
        "mix": 0.4
    },
    "Phaser": {
        "rate_hz": 0.8,
        "depth": 0.6,
        "mix": 0.5
    },
    "Compressor": {
        "threshold_db": -20.0,
        "ratio": 4.0,
        "attack_ms": 10.0
    },
    "NoiseGate": {
        "threshold_db": -40.0,
        "ratio": 10.0,
        "release_ms": 30.0
    },
    "PitchShift": {
        "semitones": 0.0
    },
    "Gain": {
        "gain_db": 0.0
    }
}


def populate_full_node_params(node: DSPNodeConfig) -> None:
    """Populates missing parameters for a DSP node with reasonable acoustic defaults."""
    defaults = NODE_PARAM_DEFAULTS.get(node.node_type, NODE_PARAM_DEFAULTS.get("Amp", {}))
    for k, v in defaults.items():
        if k not in node.params:
            node.params[k] = v


def build_pedalboard_plugins(node: DSPNodeConfig, sample_rate: int = 48000) -> List[pedalboard.Plugin]:
    """Instantiates clean, high-fidelity studio amplifier and gear pedal chain."""
    if not node.enabled:
        return []

    populate_full_node_params(node)
    p = node.params
    ntype = node.node_type

    if ntype == "Amp":
        gain = float(p.get("gain_db", p.get("drive_db", 12.0)))
        bass = float(p.get("bass_db", 0.0))
        mid = float(p.get("mid_db", 0.0))
        treble = float(p.get("treble_db", 0.0))
        presence = float(p.get("presence_db", 0.0))
        volume = float(p.get("volume_db", 0.0))

        return [
            LowShelfFilter(cutoff_frequency_hz=180.0, gain_db=bass),
            PeakFilter(cutoff_frequency_hz=750.0, gain_db=mid, q=1.0),
            HighShelfFilter(cutoff_frequency_hz=3500.0, gain_db=treble),
            PeakFilter(cutoff_frequency_hz=5500.0, gain_db=presence, q=1.2),
            Gain(gain_db=gain),
            Gain(gain_db=volume),
            Limiter(threshold_db=-0.5, release_ms=40.0)
        ]

    elif ntype == "Distortion":
        drive = float(p.get("drive_db", 12.0))
        tone = float(p.get("tone", 0.55))
        level = float(p.get("level_db", 0.0))
        return [
            Distortion(drive_db=drive),
            HighShelfFilter(cutoff_frequency_hz=3200.0, gain_db=(tone - 0.5) * 12.0),
            Gain(gain_db=level)
        ]

    elif ntype == "Delay":
        delay_sec = float(p.get("delay_seconds", 0.35))
        feedback = min(max(float(p.get("feedback", 0.45)), 0.0), 0.95)
        mix = min(max(float(p.get("mix", 0.35)), 0.0), 1.0)
        return [
            Delay(delay_seconds=delay_sec, feedback=feedback, mix=mix)
        ]

    elif ntype == "Reverb":
        room = min(max(float(p.get("room_size", 0.65)), 0.0), 1.0)
        damping = min(max(float(p.get("damping", 0.3)), 0.0), 1.0)
        wet = min(max(float(p.get("wet_level", 0.35)), 0.0), 1.0)
        dry = min(max(float(p.get("dry_level", 0.85)), 0.0), 1.0)
        width = min(max(float(p.get("width", 1.0)), 0.0), 1.0)
        return [
            Reverb(room_size=room, damping=damping, wet_level=wet, dry_level=dry, width=width)
        ]

    elif ntype == "Chorus":
        rate = float(p.get("rate_hz", 1.2))
        depth = min(max(float(p.get("depth", 0.55)), 0.0), 1.0)
        mix = min(max(float(p.get("mix", 0.4)), 0.0), 1.0)
        return [
            Chorus(rate_hz=rate, depth=depth, mix=mix)
        ]

    elif ntype == "Phaser":
        rate = float(p.get("rate_hz", 0.8))
        depth = min(max(float(p.get("depth", 0.6)), 0.0), 1.0)
        mix = min(max(float(p.get("mix", 0.5)), 0.0), 1.0)
        return [
            Phaser(rate_hz=rate, depth=depth, mix=mix)
        ]

    elif ntype == "Compressor":
        thresh = float(p.get("threshold_db", -20.0))
        ratio = max(float(p.get("ratio", 4.0)), 1.0)
        attack = max(float(p.get("attack_ms", 10.0)), 0.1)
        return [
            Compressor(threshold_db=thresh, ratio=ratio, attack_ms=attack)
        ]

    elif ntype == "NoiseGate":
        thresh = float(p.get("threshold_db", -40.0))
        ratio = max(float(p.get("ratio", 10.0)), 1.0)
        release = max(float(p.get("release_ms", 30.0)), 1.0)
        return [
            NoiseGate(threshold_db=thresh, ratio=ratio, release_ms=release)
        ]

    elif ntype == "PitchShift":
        semi = float(p.get("semitones", 0.0))
        return [
            PitchShift(semitones=semi)
        ]

    elif ntype == "Gain":
        gain = float(p.get("gain_db", 0.0))
        return [Gain(gain_db=gain)]

    return []


def build_pedalboard_chain(graph: Optional[DSPGraphConfig], sample_rate: int = 48000) -> Optional[Pedalboard]:
    """Builds a Pedalboard instance from a DSPGraphConfig. If graph is direct or empty, returns None for pure DI."""
    if graph is None or getattr(graph, "is_direct", False) or not graph.nodes:
        return None

    plugins = []
    for node in graph.nodes:
        plugins.extend(build_pedalboard_plugins(node, sample_rate=sample_rate))

    if abs(getattr(graph, "global_gain_db", 0.0)) > 0.01:
        plugins.append(Gain(gain_db=graph.global_gain_db))

    return Pedalboard(plugins) if plugins else None


class CrossfadeDSPEngine:
    """
    Real-time double-buffered DSP audio engine.
    Ensures seamless equal-power crossfading between board states to eliminate
    pops, clicks, and discontinuities during live tuning.
    """

    def __init__(self, sample_rate: int = 48000, crossfade_samples: int = 512):
        self.sample_rate = sample_rate
        self.crossfade_samples = crossfade_samples
        self.lock = threading.Lock()

        self.current_config: Optional[DSPGraphConfig] = None
        self.current_board: Optional[Pedalboard] = None
        self.target_board: Optional[Pedalboard] = None

        self.is_crossfading: bool = False
        self.crossfade_progress: int = 0

    def update_graph(self, new_graph: DSPGraphConfig) -> None:
        """Schedules a new DSP graph configuration to be crossfaded in."""
        new_board = build_pedalboard_chain(new_graph, sample_rate=self.sample_rate)
        with self.lock:
            if self.current_config is None:
                self.current_board = new_board
                self.current_config = new_graph
                self.target_board = None
                self.is_crossfading = False
                self.crossfade_progress = 0
            else:
                self.target_board = new_board
                self.current_config = new_graph
                self.is_crossfading = True
                self.crossfade_progress = 0

    def process(self, audio: np.ndarray, sample_rate: Optional[int] = None) -> np.ndarray:
        """
        Processes an audio chunk (1D or 2D: samples x channels) through the active board.
        Returns processed audio in matching input shape and dtype (float32).
        """
        sr = sample_rate or self.sample_rate
        in_shape = audio.shape
        is_1d = (audio.ndim == 1)

        # Ensure float32
        data = audio.astype(np.float32)

        # Pedalboard expects (channels, samples) or (samples,)
        if not is_1d:
            if in_shape[0] > in_shape[1]:
                # Shape is (samples, channels) -> transpose to (channels, samples)
                data_for_pb = data.T
                transposed = True
            else:
                data_for_pb = data
                transposed = False
        else:
            data_for_pb = data
            transposed = False

        num_samples = data_for_pb.shape[-1]

        with self.lock:
            cur_b = self.current_board
            tgt_b = self.target_board
            fading = self.is_crossfading
            prog = self.crossfade_progress

        if cur_b is None and tgt_b is None:
            return data

        if not fading or tgt_b is None:
            try:
                out = cur_b(data_for_pb, sr) if cur_b else data_for_pb
            except Exception as e:
                print(f"[DSP Process Error]: {e}")
                out = data_for_pb
        else:
            # Crossfade active
            try:
                out_cur = cur_b(data_for_pb, sr) if cur_b is not None else data_for_pb
                out_tgt = tgt_b(data_for_pb, sr) if tgt_b is not None else data_for_pb
            except Exception as e:
                print(f"[DSP Crossfade Error]: {e}")
                out_cur = data_for_pb
                out_tgt = data_for_pb

            total_fade = max(self.crossfade_samples, 1)
            remaining_fade = total_fade - prog
            fade_len = min(num_samples, remaining_fade)

            t = np.linspace(
                prog / total_fade,
                (prog + fade_len) / total_fade,
                fade_len,
                endpoint=False,
                dtype=np.float32
            )
            fade_out = np.cos(t * (np.pi / 2.0))
            fade_in = np.sin(t * (np.pi / 2.0))

            out = np.empty_like(out_tgt if out_tgt is not None else data_for_pb)
            if out.ndim == 1:
                out[:fade_len] = (out_cur[:fade_len] * fade_out) + (out_tgt[:fade_len] * fade_in)
                if fade_len < num_samples:
                    out[fade_len:] = out_tgt[fade_len:]
            else:
                out[:, :fade_len] = (out_cur[:, :fade_len] * fade_out[None, :]) + (out_tgt[:, :fade_len] * fade_in[None, :])
                if fade_len < num_samples:
                    out[:, fade_len:] = out_tgt[:, fade_len:]

            with self.lock:
                self.crossfade_progress += fade_len
                if self.crossfade_progress >= total_fade:
                    self.current_board = self.target_board
                    self.target_board = None
                    self.is_crossfading = False
                    self.crossfade_progress = 0

        # Convert back to original shape
        if not is_1d and transposed:
            return out.T
        return out
