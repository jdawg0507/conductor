"""
Dynamic Pydantic data schemas for Conductor AI Graph Orchestrator.
Supports dynamic parameter discovery, emotional/ambient acoustic prompts,
arbitrary pedalboard topologies, and DAW host bridges.
"""

from typing import Dict, List, Optional, Literal, Any, Union
from pydantic import BaseModel, Field


NodeType = Literal[
    "Amp",
    "Distortion",
    "Reverb",
    "Chorus",
    "Delay",
    "HighpassFilter",
    "LowpassFilter",
    "PeakFilter",
    "Compressor",
    "PitchShift",
    "Phaser",
    "Limiter",
    "NoiseGate",
    "Bitcrush",
    "Gain"
]


class DynamicParamSpec(BaseModel):
    """Specification for a dynamic parameter on a pedal/node."""
    name: str = Field(..., description="Parameter key (e.g., 'drive_db', 'bit_depth', 'cutoff_frequency_hz')")
    label: str = Field(..., description="Human-readable label (e.g., 'Drive', 'Bit Depth', 'Cutoff Frequency')")
    val: float = Field(..., description="Current value")
    min_val: float = Field(default=0.0)
    max_val: float = Field(default=1.0)
    unit: str = Field(default="", description="Display unit (e.g., 'Hz', 'dB', 'ms', '%', 'st')")


class DSPNodeConfig(BaseModel):
    """Configuration for a single audio processing plugin node."""
    node_type: NodeType = Field(..., description="Type of Pedalboard or VST DSP plugin node")
    gear_type: Optional[str] = Field(default="Amp", description="Gear classification: 'Amp', 'Stompbox', or 'Space'")
    enabled: bool = Field(default=True, description="Whether node is active in audio processing chain")
    params: Dict[str, Union[float, int]] = Field(
        default_factory=dict,
        description="Key-value mapping of parameter names to physical or normalized values"
    )
    display_name: Optional[str] = Field(default=None, description="Custom pedal/effect title (e.g., 'Cathedral Shimmer Reverb')")
    gear_model: Optional[str] = Field(default=None, description="Physical hardware modeled or TONE3000 capture model (e.g., 'Marshall JCM800 2203')")
    gear_make: Optional[str] = Field(default=None, description="Manufacturer/brand (e.g., 'Marshall', 'Vox', 'Fender', 'Roland')")
    patch_name: Optional[str] = Field(default=None, description="Stock patch or capture name from catalog")
    tone_id: Optional[str] = Field(default=None, description="TONE3000 capture unique identifier")


class DSPGraphConfig(BaseModel):
    """Complete executable DSP graph topology and state."""
    intent: str = Field(..., description="High-level sound engineering rationale or aesthetic summary")
    gear_selection_rationale: Optional[str] = Field(default=None, description="Detailed explanation of why specific gear and patch captures were chosen")
    emotional_tags: List[str] = Field(default_factory=list, description="Extracted emotional/spatial metaphors")
    global_gain_db: float = Field(default=0.0, ge=-60.0, le=18.0, description="Master output gain adjustment in dB")
    target_instrument: Optional[str] = Field(default="guitar")
    is_direct: bool = Field(default=False, description="When true, bypasses all processing for pure direct DI audio")
    nodes: List[DSPNodeConfig] = Field(
        default_factory=list,
        description="Sequential audio processing nodes forming the DSP chain"
    )
    selected_gear_summary: List[Dict[str, Any]] = Field(
        default_factory=list,
        description="Structured summary of specific gear/amps and stompboxes curated by AI"
    )

    def to_summary_dict(self) -> dict:
        return {
            "intent": self.intent,
            "gain": f"{self.global_gain_db:+.1f} dB",
            "nodes": [f"{n.display_name or n.node_type} ({'ON' if n.enabled else 'OFF'})" for n in self.nodes]
        }


class Tone3000CaptureModel(BaseModel):
    """TONE3000 Neural Amp Modeler capture profile model."""
    tone_id: str
    title: str
    author: str = "Community"
    gear_type: str = "Amp"
    amp_model: str = "Custom Amp"
    gain_level: str = "High Gain"
    tags: List[str] = Field(default_factory=list)
    default_knobs: Dict[str, float] = Field(default_factory=dict)


class GuitarAmpKnobs(BaseModel):
    """Virtual amp & pedalboard knob controls normalized [0.0 - 1.0] or explicit dB."""
    gain: float = Field(default=0.5, ge=0.0, le=1.0)
    volume: float = Field(default=0.8, ge=0.0, le=1.0)
    bass: float = Field(default=0.5, ge=0.0, le=1.0)
    mid: float = Field(default=0.5, ge=0.0, le=1.0)
    treble: float = Field(default=0.5, ge=0.0, le=1.0)
    presence: float = Field(default=0.5, ge=0.0, le=1.0)
    gain_db: Optional[float] = None
    bass_db: Optional[float] = None
    mid_db: Optional[float] = None
    treble_db: Optional[float] = None
    presence_db: Optional[float] = None
    volume_db: Optional[float] = None
    noise_gate: float = Field(default=0.0, ge=0.0, le=1.0)




class VoiceCommandRequest(BaseModel):
    """Speech or text command input to the Live AI Agent."""
    command: str = Field(..., description="Abstract, emotional, or technical prompt (e.g., 'I want to feel like floating in an underwater space station')")
    current_graph: Optional[DSPGraphConfig] = Field(default=None, description="Current active DSP graph")


class SoundEngineeringResponse(BaseModel):
    """Response containing computed tone, dynamic graph, tuning summary, and latency."""
    command: str
    intent: str
    gear_selection_rationale: Optional[str] = Field(default=None, description="Detailed explanation of specific gear and patch selections")
    emotional_tags: List[str]
    tone3000: Optional[Tone3000CaptureModel] = None
    graph: DSPGraphConfig
    latency_ms: float
    tuning_action: Optional[str] = Field(default=None, description="'board_created' or 'live_tuned'")
    changes_summary: Optional[str] = Field(default=None, description="Summary of changes made to the pedalboard")
