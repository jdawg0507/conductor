"""
Unit Test Suite for Conductor Autonomous AI Sound Designer & Dynamic DSP Synthesizer.
Focuses on Direct DI bypass and authentic TONE3000 Neural Amp Modeler patch selection.
"""

import pytest
import numpy as np
import io
import soundfile as sf

from conductor.schemas import DSPNodeConfig, DSPGraphConfig, Tone3000CaptureModel, GuitarAmpKnobs
from conductor.tone3000 import Tone3000Client, FEATURED_TONE3000_CAPTURES, tone3000_search_fallback
from conductor.guitar_engine import build_guitar_dsp_graph
from conductor.dsp_engine import CrossfadeDSPEngine, build_pedalboard_chain
from conductor.audio_io import Tone3000HardwareDuplexStream, process_audio_file


def test_tone3000_client_search():
    """Verify TONE3000 NAM capture search."""
    client = Tone3000Client()
    results = client.search_tones("marshall jcm800")
    assert len(results) > 0
    assert any("marshall" in r.amp_model.lower() or "jcm800" in r.title.lower() for r in results)


def test_direct_mode_bypass():
    """Verify Direct DI bypass creates clean pass-through with no distortion or added latency."""
    engine = CrossfadeDSPEngine(sample_rate=48000, crossfade_samples=512)
    guitar_in = np.random.uniform(-0.3, 0.3, (512, 2)).astype(np.float32)

    # Configure Direct DI
    direct_graph = DSPGraphConfig(
        intent="Direct DI (Clean Bypass)",
        is_direct=True,
        nodes=[]
    )
    engine.update_graph(direct_graph)

    # Verify board chain is None (pure direct pass-through)
    assert build_pedalboard_chain(direct_graph) is None

    # Process samples
    out = engine.process(guitar_in)
    assert out.shape == guitar_in.shape
    # Complete crossfade and check bit-exact passthrough
    out2 = engine.process(guitar_in)
    np.testing.assert_allclose(out2, guitar_in, atol=1e-5)


def test_patch_selection_and_tonestack():
    """Verify authentic TONE3000 patch creates Amp node with studio tonestack."""
    cap_data = FEATURED_TONE3000_CAPTURES[0]
    tone = Tone3000CaptureModel(**cap_data)
    knobs = GuitarAmpKnobs(gain_db=12.0, bass_db=2.0, mid_db=1.0, treble_db=3.0, presence_db=2.0, volume_db=0.0)

    graph = build_guitar_dsp_graph(tone=tone, knobs=knobs, intent="Cranked 80s Rock")
    assert graph is not None
    assert graph.is_direct is False
    assert len(graph.nodes) == 1
    assert graph.nodes[0].node_type == "Amp"
    assert graph.nodes[0].params["gain_db"] == 12.0
    assert graph.nodes[0].params["treble_db"] == 3.0


def test_conversational_tonestack_tuning():
    """Verify conversational tuning adjusts amp tonestack parameters smoothly."""
    # 1. Initial rock tone
    initial_graph = tone3000_search_fallback("cranked 80s rock lead")
    assert initial_graph is not None
    amp_node = next(n for n in initial_graph.nodes if n.node_type == "Amp")
    initial_gain = amp_node.params["gain_db"]
    initial_treble = amp_node.params["treble_db"]

    # 2. Conversational prompt: "more gain and darker"
    tuned_graph = tone3000_search_fallback("more gain and darker", current_graph=initial_graph)
    tuned_amp = next(n for n in tuned_graph.nodes if n.node_type == "Amp")
    assert tuned_amp.params["gain_db"] > initial_gain
    assert tuned_amp.params["treble_db"] < initial_treble


def test_dsp_engine_crossfade_direct_to_patch():
    """Verify equal-power crossfader transitions smoothly between Direct DI and TONE3000 patch."""
    engine = CrossfadeDSPEngine(sample_rate=48000, crossfade_samples=512)
    guitar_in = np.random.uniform(-0.2, 0.2, (512, 2)).astype(np.float32)

    # 1. Direct DI
    direct_graph = DSPGraphConfig(intent="Direct DI", is_direct=True, nodes=[])
    engine.update_graph(direct_graph)
    engine.process(guitar_in)

    # 2. Switch to TONE3000 Patch
    patch_graph = DSPGraphConfig(
        intent="TONE3000 JCM800",
        is_direct=False,
        nodes=[DSPNodeConfig(node_type="Amp", gear_model="Marshall JCM800", enabled=True, params={"gain_db": 10.0})]
    )
    engine.update_graph(patch_graph)
    assert engine.is_crossfading is True

    # Process crossfade chunk
    out = engine.process(guitar_in)
    assert out.shape == (512, 2)
    assert engine.is_crossfading is False


def test_server_patch_and_knob_api():
    """Verify server endpoints for patch browsing, direct toggle, and real-time knob adjustments."""
    from conductor.server import (
        set_server_dsp_engine,
        get_tone3000_patches,
        select_patch_endpoint,
        set_direct_mode,
        set_amp_knobs,
        search_tones
    )

    engine = CrossfadeDSPEngine(sample_rate=48000)
    set_server_dsp_engine(engine)

    # 1. Get patches
    patches_res = get_tone3000_patches()
    assert patches_res["status"] == "ok"
    assert len(patches_res["patches"]) >= 6

    # 2. Select patch
    select_res = select_patch_endpoint({"tone_id": "t3k-jcm800-80s"})
    assert select_res["status"] == "patch_selected"
    assert engine.current_config.is_direct is False
    assert "JCM800" in select_res["patch"]["title"]

    # 3. Direct mode toggle
    direct_res = set_direct_mode({"is_direct": True})
    assert direct_res["status"] == "ok"
    assert direct_res["is_direct"] is True
    assert engine.current_config.is_direct is True

    # Toggle back to patch
    set_direct_mode({"is_direct": False})
    assert engine.current_config.is_direct is False

    # 4. Set knobs in real-time
    knob_res = set_amp_knobs({"gain_db": 18.0, "bass_db": 4.5, "treble_db": 2.0})
    assert knob_res["status"] == "ok"
    amp_node = next(n for n in engine.current_config.nodes if n.node_type == "Amp")
    assert amp_node.params["gain_db"] == 18.0
    assert amp_node.params["bass_db"] == 4.5
    assert amp_node.params["treble_db"] == 2.0

    # 5. Search TONE3000 tones
    search_res = search_tones(query="Plexi")
    assert search_res["status"] == "ok"
    assert len(search_res["results"]) > 0


def test_hardware_duplex_callback_processing():
    """Verify Tone3000HardwareDuplexStream feeds guitar signal into DSP engine."""
    engine = CrossfadeDSPEngine(sample_rate=48000)
    graph = DSPGraphConfig(
        intent="Direct DI Test",
        is_direct=True,
        nodes=[]
    )
    engine.update_graph(graph)
    engine.process(np.zeros((512, 2), dtype=np.float32))

    stream = Tone3000HardwareDuplexStream(dsp_engine=engine, mic_ch=0, guitar_ch=1)
    indata = np.random.uniform(-0.2, 0.2, (512, 2)).astype(np.float32)
    outdata = np.zeros((512, 2), dtype=np.float32)

    stream._audio_callback(indata, outdata, 512, None, None)
    # In direct mode with guitar on ch 1, mono guitar should be routed to stereo output
    assert np.max(np.abs(outdata)) > 0.0


def test_process_audio_file():
    """Verify process_audio_file processes in-memory WAV bytes with studio tonestack."""
    engine = CrossfadeDSPEngine(sample_rate=48000)
    graph = DSPGraphConfig(
        intent="Studio Clean Amp",
        is_direct=False,
        nodes=[DSPNodeConfig(node_type="Amp", enabled=True, params={"gain_db": 6.0, "volume_db": -1.0})]
    )
    engine.update_graph(graph)

    # Create dummy in-memory WAV
    sample_rate = 48000
    duration = 0.3
    t = np.linspace(0, duration, int(sample_rate * duration), endpoint=False)
    sig = (0.3 * np.sin(2 * np.pi * 440.0 * t)).astype(np.float32)
    stereo = np.column_stack([sig, sig])

    in_buf = io.BytesIO()
    sf.write(in_buf, stereo, sample_rate, format="WAV")
    raw_wav_bytes = in_buf.getvalue()

    out_wav_bytes, dur = process_audio_file(raw_wav_bytes, engine, sample_rate=sample_rate)
    assert len(out_wav_bytes) > 0
    assert dur > 0.25
