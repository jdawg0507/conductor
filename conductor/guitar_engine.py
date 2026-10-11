"""
Guitar Engine helper for compiling TONE3000 capture profiles and physical knob controls
into DSPGraphConfig topologies.
"""

from typing import Optional
from .schemas import DSPGraphConfig, DSPNodeConfig, Tone3000CaptureModel, GuitarAmpKnobs
from .dsp_engine import populate_full_node_params


def build_guitar_dsp_graph(
    tone: Tone3000CaptureModel,
    knobs: GuitarAmpKnobs,
    intent: Optional[str] = None,
    pedals: Optional[list] = None
) -> DSPGraphConfig:
    """
    Translates a TONE3000 capture profile and normalized knob settings [0.0 - 1.0]
    into a complete DSPGraphConfig with an Amp node and complementary pedals.
    """
    nodes = []

    # Stompboxes placed before amp
    if pedals:
        for p in pedals:
            if getattr(p, "gear_type", "Stompbox") == "Stompbox":
                nodes.append(p)

    # Core Amp & Cab stage from Tone3000 capture
    drive_db = float(knobs.gain_db if knobs.gain_db is not None else (knobs.gain * 36.0 - 6.0))
    bass_db = float(knobs.bass_db if knobs.bass_db is not None else ((knobs.bass - 0.5) * 16.0))
    mid_db = float(knobs.mid_db if knobs.mid_db is not None else ((knobs.mid - 0.5) * 16.0))
    treble_db = float(knobs.treble_db if knobs.treble_db is not None else ((knobs.treble - 0.5) * 16.0))
    presence_db = float(knobs.presence_db if knobs.presence_db is not None else ((knobs.presence - 0.5) * 16.0))
    volume_db = float(knobs.volume_db if knobs.volume_db is not None else ((knobs.volume - 0.5) * 12.0))

    amp_node = DSPNodeConfig(
        node_type="Amp",
        gear_type="Amp",
        enabled=True,
        display_name=tone.title,
        gear_model=tone.amp_model,
        gear_make=tone.author,
        patch_name=tone.title,
        tone_id=tone.tone_id,
        params={
            "gain_db": drive_db,
            "drive_db": drive_db,
            "bass_db": bass_db,
            "mid_db": mid_db,
            "treble_db": treble_db,
            "presence_db": presence_db,
            "volume_db": volume_db
        }
    )
    populate_full_node_params(amp_node)
    nodes.append(amp_node)

    # Spaces placed after amp
    if pedals:
        for p in pedals:
            if getattr(p, "gear_type", "") == "Space":
                nodes.append(p)

    return DSPGraphConfig(
        intent=intent or f"TONE3000: {tone.title}",
        gear_selection_rationale=f"Selected {tone.title} ({tone.gain_level}) modeling {tone.amp_model}",
        emotional_tags=tone.tags or ["tone3000", "guitar"],
        global_gain_db=0.0,
        target_instrument="guitar",
        is_direct=False,
        nodes=nodes
    )
