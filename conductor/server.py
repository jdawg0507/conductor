"""
FastAPI Server providing REST API and Web Dashboard for Conductor Autonomous AI Sound Designer.
Includes 2 Inputs -> 1 Output Routing & Hardware Audio Interface Settings Endpoints.
"""

from fastapi import FastAPI, HTTPException, Request, UploadFile, File, Response
from fastapi.staticfiles import StaticFiles
from fastapi.responses import HTMLResponse, JSONResponse
from fastapi.middleware.cors import CORSMiddleware
import uvicorn
import os
import time
import numpy as np

from typing import Dict, List, Optional, Any
from .config import config
from .schemas import VoiceCommandRequest, SoundEngineeringResponse, DSPGraphConfig, Tone3000CaptureModel, GuitarAmpKnobs
from .nebius_client import NebiusSoundEngineerClient
from .dsp_engine import CrossfadeDSPEngine
from .tone3000 import FEATURED_TONE3000_CAPTURES, Tone3000Client
from .audio_io import get_available_devices, Tone3000HardwareDuplexStream, process_audio_file
from .guitar_engine import build_guitar_dsp_graph


app = FastAPI(
    title="Conductor AI Sound Designer",
    description="Autonomous AI Sound Designer & Dynamic DSP Synthesizer",
    version="2.0.0"
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Global Engine Instances
dsp_engine = CrossfadeDSPEngine(sample_rate=config.sample_rate)
nebius_client = NebiusSoundEngineerClient()
tone3000_client = Tone3000Client()


core_engine = None


def set_server_core_engine(engine):
    """Binds main Conductor execution engine to server for live hardware stream control."""
    global core_engine, dsp_engine
    core_engine = engine
    if engine and hasattr(engine, "dsp_engine"):
        dsp_engine = engine.dsp_engine


def set_server_dsp_engine(engine: CrossfadeDSPEngine):
    """Share active hardware audio duplex stream DSP engine with REST API."""
    global dsp_engine
    dsp_engine = engine


def get_server_dsp_engine() -> CrossfadeDSPEngine:
    return dsp_engine


@app.get("/api/health")
def health_check():
    return {
        "status": "online",
        "engine": "Conductor AI Sound Designer",
        "sample_rate": config.sample_rate,
        "nebius_connected": bool(nebius_client.api_key),
        "target_max_latency_ms": config.target_max_latency_ms
    }


@app.get("/api/audio_devices")
def get_audio_devices():
    """Returns detected system audio devices, defaults, current routing, and active stream status."""
    dev_info = get_available_devices()
    stream_status = {}
    if core_engine and hasattr(core_engine, "hardware_stream"):
        stream_status = core_engine.hardware_stream.get_status()

    return {
        "devices": dev_info.get("devices", []),
        "default_input": dev_info.get("default_input"),
        "default_output": dev_info.get("default_output"),
        "current_config": {
            "input_device_id": config.guitar_device_id,
            "output_device_id": config.speaker_device_id,
            "guitar_channel_idx": config.guitar_channel_idx,
            "mic_channel_idx": config.mic_channel_idx,
            "mic_enabled": getattr(config, "mic_enabled", True)
        },
        "stream_status": stream_status
    }


@app.post("/api/set_audio_device")
async def set_audio_device(req: Request):
    """Configures input interface, output interface, and channel pins with live stream restart."""
    data = await req.json()

    def parse_dev_id(val):
        if val is None or val == "" or val == "null" or val == "default":
            return None
        if isinstance(val, int):
            return val
        if isinstance(val, str) and val.isdigit():
            return int(val)
        return val

    in_dev = parse_dev_id(data.get("input_device_id", data.get("guitar_device_id")))
    out_dev = parse_dev_id(data.get("output_device_id", data.get("speaker_device_id")))
    mic_ch = int(data.get("mic_channel_idx", 0))
    guitar_ch = int(data.get("guitar_channel_idx", 1))
    mic_enabled = bool(data.get("mic_enabled", True))

    config.guitar_device_id = in_dev
    config.mic_device_id = in_dev
    config.speaker_device_id = out_dev
    config.mic_channel_idx = mic_ch
    config.guitar_channel_idx = guitar_ch

    stream_restarted = False
    stream_status = {}
    if core_engine and hasattr(core_engine, "hardware_stream"):
        stream_restarted = core_engine.hardware_stream.reconfigure(
            input_device_id=in_dev,
            output_device_id=out_dev,
            mic_ch=mic_ch,
            guitar_ch=guitar_ch,
            mic_enabled=mic_enabled
        )
        stream_status = core_engine.hardware_stream.get_status()

    return {
        "status": "updated",
        "stream_restarted": stream_restarted,
        "input_device_id": in_dev,
        "output_device_id": out_dev,
        "mic_channel_idx": mic_ch,
        "guitar_channel_idx": guitar_ch,
        "mic_enabled": mic_enabled,
        "stream_status": stream_status
    }


@app.get("/api/test_audio_files")
def get_test_audio_files():
    """Lists available WAV test files in the test_input directory."""
    test_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), "test_input")
    files = []
    if os.path.exists(test_dir):
        for f in os.listdir(test_dir):
            if f.lower().endswith((".wav", ".mp3", ".flac")):
                files.append({
                    "filename": f,
                    "name": f.replace(".wav", "").replace(".mp3", "")
                })
    return {"files": files}


@app.get("/api/test_audio_stream/{filename}")
def stream_test_audio(filename: str):
    """Streams raw test WAV audio file to the browser player."""
    from fastapi.responses import FileResponse
    test_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), "test_input")
    file_path = os.path.join(test_dir, filename)
    if not os.path.exists(file_path):
        raise HTTPException(status_code=404, detail="Test audio file not found")
    return FileResponse(file_path, media_type="audio/wav")


@app.post("/api/process_test_audio")
async def process_test_audio(req: Request):
    """Processes selected test WAV file through active DSP engine and returns processed audio bytes."""
    data = await req.json()
    filename = data.get("filename", "Dry Guitar.wav")
    test_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), "test_input")
    file_path = os.path.join(test_dir, filename)

    if not os.path.exists(file_path):
        raise HTTPException(status_code=404, detail=f"File {filename} not found in test_input")

    with open(file_path, "rb") as f:
        file_bytes = f.read()

    wav_bytes, duration = process_audio_file(file_bytes, dsp_engine, sample_rate=config.sample_rate)
    return Response(content=wav_bytes, media_type="audio/wav")


@app.get("/api/featured_tones")
def get_featured_tones():
    return {"tones": FEATURED_TONE3000_CAPTURES}


@app.get("/api/search_tones")
def search_tones(query: str = ""):
    """Searches live TONE3000 platform API and featured library for guitar amp/pedal captures."""
    if not query or not query.strip():
        raw_items = [c.model_dump() if hasattr(c, "model_dump") else c for c in FEATURED_TONE3000_CAPTURES]
    else:
        captures = tone3000_client.search_tones(query.strip())
        raw_items = [c.model_dump() if hasattr(c, "model_dump") else c for c in captures]

    items = []
    for item in raw_items:
        d = dict(item)
        d["name"] = d.get("title") or d.get("name", "")
        d["id"] = d.get("tone_id") or d.get("id", "")
        items.append(d)

    return {"status": "ok", "tones": items, "results": items}


@app.get("/api/tone3000_patches")
def get_tone3000_patches():
    """Returns curated iconic TONE3000 amplifier and drive captures."""
    amps_only = [c for c in FEATURED_TONE3000_CAPTURES if c["gear_type"] == "Amp"]
    return {"status": "ok", "patches": amps_only}


@app.get("/api/gear_catalog")
def get_gear_catalog():
    """Returns all available TONE3000 gear partitioned into Amps, Stompboxes, and Spaces."""
    amps = [c for c in FEATURED_TONE3000_CAPTURES if c["gear_type"] == "Amp"]
    stompboxes = [c for c in FEATURED_TONE3000_CAPTURES if c["gear_type"] == "Stompbox"]
    spaces = [c for c in FEATURED_TONE3000_CAPTURES if c["gear_type"] == "Space"]
    return {
        "status": "ok",
        "amps": amps,
        "stompboxes": stompboxes,
        "spaces": spaces,
        "all": FEATURED_TONE3000_CAPTURES
    }


@app.get("/api/telemetry")
def get_telemetry():
    """Returns real-time dBFS meters (input/output), clipping latch, and guitar pitch tuner data."""
    if core_engine and hasattr(core_engine, "hardware_stream") and core_engine.hardware_stream:
        return core_engine.hardware_stream.get_telemetry()
    return {
        "input_peak_db": -60.0,
        "input_rms_db": -60.0,
        "output_peak_db": -60.0,
        "output_rms_db": -60.0,
        "input_clipping": False,
        "output_clipping": False,
        "tuner": {"detected": False, "note": "--", "octave": 0, "freq_hz": 0.0, "cents": 0.0, "in_tune": False}
    }


@app.post("/api/toggle_gear")
def toggle_gear(payload: Dict[str, Any]):
    """Toggles bypass/enabled state of a specific gear pedal/amp node by index or tone_id."""
    curr = dsp_engine.current_config
    if not curr or not curr.nodes:
        return {"status": "error", "message": "No active graph"}

    idx = payload.get("index")
    tone_id = payload.get("tone_id")

    if idx is not None and 0 <= idx < len(curr.nodes):
        curr.nodes[idx].enabled = not curr.nodes[idx].enabled
    elif tone_id:
        for node in curr.nodes:
            if node.tone_id == tone_id:
                node.enabled = not node.enabled
                break

    dsp_engine.update_graph(curr)
    return {"status": "ok", "graph": curr}


@app.post("/api/set_gear_param")
def set_gear_param(payload: Dict[str, Any]):
    """Sets a specific parameter on any gear node in the chain."""
    curr = dsp_engine.current_config
    if not curr or not curr.nodes:
        return {"status": "error", "message": "No active graph"}

    idx = payload.get("index")
    param = payload.get("param")
    val = float(payload.get("value", 0.0))

    if idx is not None and 0 <= idx < len(curr.nodes) and param:
        curr.nodes[idx].params[param] = val
        if curr.nodes[idx].node_type == "Amp":
            if param == "gain_db":
                curr.nodes[idx].params["drive_db"] = val
            elif param == "drive_db":
                curr.nodes[idx].params["gain_db"] = val
        dsp_engine.update_graph(curr)
        return {"status": "ok", "graph": curr}

    return {"status": "error", "message": "Invalid index or param"}


@app.post("/api/add_gear")
def add_gear(payload: Dict[str, Any]):
    """Adds a stompbox or space from the TONE3000 catalog to the active signal chain."""
    from .schemas import DSPNodeConfig
    from .dsp_engine import populate_full_node_params

    curr = dsp_engine.current_config
    if not curr:
        curr = DSPGraphConfig(intent="Custom Rig", nodes=[])

    tone_id = payload.get("tone_id")
    match = next((c for c in FEATURED_TONE3000_CAPTURES if c["tone_id"] == tone_id), None)
    if not match:
        raise HTTPException(status_code=404, detail="Gear capture not found")

    node_type_map = {
        "Amp": "Amp",
        "Stompbox": "Distortion" if any(t in match["tags"] for t in ["distortion", "fuzz", "overdrive", "screamer", "boost"]) else (
            "Chorus" if "chorus" in match["tags"] else (
                "Phaser" if "phaser" in match["tags"] else (
                    "Delay" if "delay" in match["tags"] else (
                        "Compressor" if "compressor" in match["tags"] else (
                            "NoiseGate" if "noise gate" in match["tags"] else "Distortion"
                        )
                    )
                )
            )
        ),
        "Space": "Reverb"
    }

    new_node = DSPNodeConfig(
        node_type=node_type_map.get(match["gear_type"], "Distortion"),
        gear_type=match["gear_type"],
        enabled=True,
        display_name=match["title"],
        gear_model=match["amp_model"],
        gear_make=match["author"],
        patch_name=match["title"],
        tone_id=match["tone_id"],
        params=dict(match.get("default_knobs", {}))
    )
    populate_full_node_params(new_node)

    # Insert in order: Stompboxes before Amp, Spaces after Amp
    if new_node.gear_type == "Stompbox":
        amp_idx = next((i for i, n in enumerate(curr.nodes) if n.node_type == "Amp"), len(curr.nodes))
        curr.nodes.insert(amp_idx, new_node)
    else:
        curr.nodes.append(new_node)

    dsp_engine.update_graph(curr)
    return {"status": "ok", "graph": curr}


@app.post("/api/remove_gear")
def remove_gear(payload: Dict[str, Any]):
    """Removes a pedal or space node from the signal chain (Amp cannot be removed)."""
    curr = dsp_engine.current_config
    if not curr or not curr.nodes:
        return {"status": "error", "message": "No active graph"}

    idx = payload.get("index")
    if idx is not None and 0 <= idx < len(curr.nodes):
        if curr.nodes[idx].node_type == "Amp":
            raise HTTPException(status_code=400, detail="Cannot remove mandatory Amp stage")
        curr.nodes.pop(idx)
        dsp_engine.update_graph(curr)
        return {"status": "ok", "graph": curr}

    return {"status": "error", "message": "Invalid index"}


@app.post("/api/select_patch")
def select_patch_endpoint(payload: Dict[str, Any]):
    """Selects an authentic TONE3000 amp capture and configures the amplifier tonestack."""
    tone_id = payload.get("tone_id", "")
    match = None
    for cap in FEATURED_TONE3000_CAPTURES:
        if cap["tone_id"] == tone_id:
            match = cap
            break

    if match is None:
        caps = tone3000_client.search_tones(tone_id, gear_type="Amp")
        if caps:
            match = caps[0].model_dump()
        else:
            match = FEATURED_TONE3000_CAPTURES[0]

    tone_model = Tone3000CaptureModel(**match)
    knob_dict = payload.get("knobs", match.get("default_knobs", {}))
    knobs = GuitarAmpKnobs(**knob_dict) if knob_dict else GuitarAmpKnobs()

    # Preserve any active pedals when switching amp
    existing_pedals = [n for n in (dsp_engine.current_config.nodes if dsp_engine.current_config else []) if n.node_type != "Amp"]
    graph = build_guitar_dsp_graph(tone=tone_model, knobs=knobs, intent=f"TONE3000: {match['title']}", pedals=existing_pedals)
    dsp_engine.update_graph(graph)

    return {
        "status": "patch_selected",
        "patch": match,
        "graph": graph,
        "is_direct": False
    }


@app.post("/api/set_direct_mode")
def set_direct_mode(payload: Dict[str, Any]):
    """Enables or disables Direct DI bypass mode."""
    enable_direct = bool(payload.get("is_direct", True))
    curr = dsp_engine.current_config
    if curr is None:
        curr = DSPGraphConfig(intent="Direct DI (Clean Bypass)", is_direct=enable_direct, nodes=[])
    else:
        curr = DSPGraphConfig(
            intent="Direct DI (Clean Bypass)" if enable_direct else (curr.intent or "TONE3000 Active"),
            gear_selection_rationale=curr.gear_selection_rationale,
            emotional_tags=curr.emotional_tags,
            global_gain_db=curr.global_gain_db,
            is_direct=enable_direct,
            nodes=curr.nodes
        )
    dsp_engine.update_graph(curr)
    return {
        "status": "ok",
        "is_direct": enable_direct,
        "graph": dsp_engine.current_config
    }


@app.post("/api/set_knobs")
def set_amp_knobs(payload: Dict[str, Any]):
    """Directly modifies physical amp tonestack parameters (gain_db, bass_db, mid_db, treble_db, presence_db, volume_db)."""
    curr = dsp_engine.current_config
    if not curr or not curr.nodes:
        return select_patch_endpoint({"tone_id": "t3k-jcm800-80s", "knobs": payload})

    for node in curr.nodes:
        if node.node_type == "Amp":
            for k in ["gain_db", "drive_db", "bass_db", "mid_db", "treble_db", "presence_db", "volume_db"]:
                if k in payload:
                    node.params[k] = float(payload[k])
            if "gain_db" in payload and "drive_db" not in payload:
                node.params["drive_db"] = float(payload["gain_db"])
            elif "drive_db" in payload and "gain_db" not in payload:
                node.params["gain_db"] = float(payload["drive_db"])

    dsp_engine.update_graph(curr)
    return {
        "status": "ok",
        "graph": curr
    }


@app.get("/api/tone3000_tags")
def get_tone3000_tags():
    return {"tags": tone3000_client.get_all_tags()}


@app.get("/api/current_graph")
def get_current_graph():
    return {
        "graph": dsp_engine.current_config,
        "is_crossfading": dsp_engine.is_crossfading
    }


@app.post("/api/process_command", response_model=SoundEngineeringResponse)
def process_aesthetic_prompt(req: VoiceCommandRequest):
    """
    Processes abstract, emotional, spatial, or technical prompt,
    synthesizes or live-tunes dynamic DSP graph via Nebius AI, and mutates DSP engine.
    """
    t0 = time.perf_counter()

    res = nebius_client.process_command(
        command=req.command,
        current_graph=req.current_graph or dsp_engine.current_config
    )
    graph = res.graph

    # Mutate Pedalboard DSP Engine with click-free crossfading
    dsp_engine.update_graph(graph)

    total_latency = (time.perf_counter() - t0) * 1000.0

    return SoundEngineeringResponse(
        command=req.command,
        intent=graph.intent,
        gear_selection_rationale=graph.gear_selection_rationale,
        emotional_tags=graph.emotional_tags or ["ambient"],
        graph=graph,
        latency_ms=round(total_latency, 2),
        tuning_action=res.tuning_action,
        changes_summary=res.changes_summary
    )


@app.post("/api/update_graph")
def update_dsp_graph(graph: DSPGraphConfig):
    """Mutates active hardware DSP engine with modified graph parameters."""
    dsp_engine.update_graph(graph)
    return {"status": "updated", "graph": dsp_engine.current_config or graph}


@app.post("/api/select_tone")
def select_tone3000_capture(payload: Dict[str, Any]):
    """Selects a TONE3000 capture profile and compiles it into an active DSP graph."""
    tone_id = payload.get("tone_id", "")
    knob_dict = payload.get("knobs", {})

    match = None
    for cap in FEATURED_TONE3000_CAPTURES:
        if cap["tone_id"] == tone_id:
            match = cap
            break

    if match is None:
        match = FEATURED_TONE3000_CAPTURES[0]

    tone_model = Tone3000CaptureModel(**match)
    knobs = GuitarAmpKnobs(**knob_dict) if knob_dict else GuitarAmpKnobs(**match.get("default_knobs", {}))

    graph = build_guitar_dsp_graph(tone=tone_model, knobs=knobs, intent=f"TONE3000: {match['title']}")
    dsp_engine.update_graph(graph)

    return {
        "status": "tone_selected",
        "tone": match,
        "graph": graph
    }


@app.post("/api/upload_audio_file")
async def upload_audio_file(file: UploadFile = File(...)):
    file_bytes = await file.read()
    wav_bytes, duration = process_audio_file(file_bytes, dsp_engine, sample_rate=config.sample_rate)

    return Response(content=wav_bytes, media_type="audio/wav")


# Mount Web Directory
web_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), "web")
if os.path.exists(web_dir):
    app.mount("/static", StaticFiles(directory=web_dir), name="static")

    @app.get("/", response_class=HTMLResponse)
    def read_root():
        index_file = os.path.join(web_dir, "index.html")
        if os.path.exists(index_file):
            with open(index_file, "r") as f:
                return f.read()
        return "Conductor AI Sound Designer Online."


def start_server(
    host: str = "127.0.0.1",
    port: int = 7860,
    dsp_engine_instance: Optional[CrossfadeDSPEngine] = None,
    core_engine_instance: Optional[Any] = None
):
    if core_engine_instance is not None:
        set_server_core_engine(core_engine_instance)
    elif dsp_engine_instance is not None:
        set_server_dsp_engine(dsp_engine_instance)
    uvicorn.run(app, host=host, port=port, log_level="info")
