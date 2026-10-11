"""
Nebius Token Factory API Client enforcing structured tool and JSON calls for semantic sound engineering.
Supports Meta-Llama-3.1-70B-Instruct, Mistral-Large, DeepSeek, and local offline TONE3000 fallback.
Enforces two modes: Initial Pedalboard Build & Conversational Live Tuning with on-the-fly pedal swapping.
Acts as a creative springboard for inspiration.
"""

import json
import time
import os
import re
from typing import Dict, Any, Optional, Tuple, List
import openai

from .config import config
from .schemas import DSPGraphConfig, DSPNodeConfig, Tone3000CaptureModel
from .prompts import GUITAR_AI_SYSTEM_PROMPT
from .tone3000 import Tone3000Client, tone3000_search_fallback, semantic_search_dictionary
from .dsp_engine import populate_full_node_params


TOOL_DEFINITION = {
    "type": "function",
    "function": {
        "name": "apply_dsp_graph",
        "description": "Mutate real-time audio DSP graph topology and parameters based on abstract, emotional, spatial, or gear intent.",
        "parameters": {
            "type": "object",
            "properties": {
                "intent": {
                    "type": "string",
                    "description": "High-level sound engineering rationale explaining the graph configuration"
                },
                "gear_selection_rationale": {
                    "type": "string",
                    "description": "Detailed explanation of why this specific amplifier capture and pedal pairing was chosen"
                },
                "tuning_action": {
                    "type": "string",
                    "enum": ["board_created", "live_tuned"],
                    "description": "Whether a brand new board was built or an existing board was live-tuned/swapped"
                },
                "changes_summary": {
                    "type": "string",
                    "description": "Brief 1-2 sentence description of what parameters were tuned or pedals swapped"
                },
                "emotional_tags": {
                    "type": "array",
                    "items": { "type": "string" },
                    "description": "Extracted emotional/spatial metaphors (e.g. ['ambient', 'cathedral', 'underwater'])"
                },
                "global_gain_db": {
                    "type": "number",
                    "minimum": -60.0,
                    "maximum": 18.0,
                    "description": "Master output gain adjustment in dB"
                },
                "nodes": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "properties": {
                            "node_type": {
                                "type": "string",
                                "enum": [
                                    "Amp", "Distortion", "Reverb", "Chorus", "Delay",
                                    "HighpassFilter", "LowpassFilter", "PeakFilter",
                                    "Compressor", "PitchShift", "Phaser", "Limiter", "NoiseGate", "Bitcrush", "Gain"
                                ]
                            },
                            "enabled": { "type": "boolean", "default": True },
                            "display_name": { "type": "string", "description": "Creative or aesthetic pedal name" },
                            "gear_model": { "type": "string", "description": "Physical hardware modeled or TONE3000 model (e.g. 'Marshall JCM800 2203')" },
                            "gear_make": { "type": "string", "description": "Manufacturer (e.g. 'Marshall', 'Vox', 'Roland', 'Fender')" },
                            "patch_name": { "type": "string", "description": "Stock patch or capture name" },
                            "tone_id": { "type": "string", "description": "TONE3000 capture unique identifier" },
                            "params": {
                                "type": "object",
                                "additionalProperties": { "type": "number" }
                            }
                        },
                        "required": ["node_type", "enabled", "params"]
                    }
                }
            },
            "required": ["intent", "nodes"]
        }
    }
}


def _extract_json_from_text(text: str) -> Optional[Dict[str, Any]]:
    """Robustly extracts and parses JSON dictionary from LLM response text."""
    if not text or not text.strip():
        return None

    cleaned = text.strip()
    try:
        data = json.loads(cleaned)
        if isinstance(data, dict):
            return data
    except Exception:
        pass

    # Extract ```json ... ``` markdown block
    match_fence = re.search(r"```(?:json)?\s*(\{.*?\})\s*```", cleaned, re.DOTALL)
    if match_fence:
        try:
            data = json.loads(match_fence.group(1))
            if isinstance(data, dict):
                return data
        except Exception:
            pass

    # Extract outermost { ... }
    start = cleaned.find("{")
    end = cleaned.rfind("}")
    if start != -1 and end != -1 and end > start:
        try:
            data = json.loads(cleaned[start:end + 1])
            if isinstance(data, dict):
                return data
        except Exception:
            pass

    return None


class NebiusSoundEngineerResult(Tuple[DSPGraphConfig, float]):
    """Tuple subclass allowing backward-compatible (graph, latency) unpacking while exposing tuning metadata."""
    def __new__(cls, graph: DSPGraphConfig, latency_ms: float, tuning_action: str = "board_created", changes_summary: str = ""):
        return super().__new__(cls, (graph, latency_ms))

    def __init__(self, graph: DSPGraphConfig, latency_ms: float, tuning_action: str = "board_created", changes_summary: str = ""):
        self.graph = graph
        self.latency_ms = latency_ms
        self.tuning_action = tuning_action
        self.changes_summary = changes_summary


class NebiusSoundEngineerClient:
    """
    Autonomous AI Sound Engineer client for Nebius Token Factory API.
    Translates abstract, emotional, spatial, or technical prompts into dynamic DSP Graph configurations.
    Supports both Initial Board Build and Conversational Live Tuning.
    """

    def __init__(self, api_key: Optional[str] = None, base_url: Optional[str] = None, model: Optional[str] = None):
        self.api_key = api_key or config.nebius_api_key or os.getenv("NEBIUS_API_KEY", "")
        self.base_url = base_url or config.nebius_base_url
        self.model = model or config.nebius_llm_model
        self.tone3000_client = Tone3000Client()

        if self.api_key:
            self.openai_client = openai.OpenAI(
                api_key=self.api_key,
                base_url=self.base_url
            )
        else:
            self.openai_client = None

    def process_command(
        self,
        command: str,
        current_graph: Optional[DSPGraphConfig] = None,
        instrument_context: str = "guitar",
        conversation_history: Optional[List[Dict[str, str]]] = None
    ) -> NebiusSoundEngineerResult:
        """
        Sends user command to Nebius LLM.
        - If current_graph has nodes and prompt is tuning/swapping: tunes existing rig.
        - If no current_graph: builds initial pedalboard using Tone3000 catalog.
        Returns NebiusSoundEngineerResult(graph, latency_ms, tuning_action, changes_summary).
        """
        start_time = time.perf_counter()

        has_active_board = bool(current_graph and current_graph.nodes and len(current_graph.nodes) > 0)

        if not self.openai_client or not self.api_key:
            graph = tone3000_search_fallback(command, instrument=instrument_context, current_graph=current_graph)
            elapsed_ms = (time.perf_counter() - start_time) * 1000.0
            action = "live_tuned" if (has_active_board and "Live Tuned" in graph.intent) else "board_created"
            summary = "Adjusted parameters on active pedalboard." if action == "live_tuned" else "Built initial board from TONE3000."
            return NebiusSoundEngineerResult(graph, elapsed_ms, tuning_action=action, changes_summary=summary)

        # Build prompt context with real live Tone3000 captures across Amps, Stompboxes, and Spaces
        matching_amps = self.tone3000_client.search_tones(command, gear_type="Amp")
        matching_pedals = self.tone3000_client.search_tones(command, gear_type="Stompbox")
        matching_spaces = self.tone3000_client.search_tones(command, gear_type="Space")
        all_tags = self.tone3000_client.get_all_tags()

        gear_inventory = []
        for c in (matching_amps[:4] + matching_pedals[:4] + matching_spaces[:3]):
            gear_inventory.append({
                "tone_id": c.tone_id,
                "title": c.title,
                "gear_type": c.gear_type,
                "gear_model": c.amp_model,
                "gain_level": c.gain_level,
                "tags": c.tags,
                "default_preset_knobs": getattr(c, "default_knobs", {})
            })

        if has_active_board:
            mode_instruction = (
                "MODE: CONVERSATIONAL LIVE TUNING ON ACTIVE RIG.\n"
                "The user is giving live playing feedback. Keep the existing gear topology and knob settings "
                "EXCEPT for the specific changes, swaps, additions, or adjustments the user requested."
            )
            current_state_summary = f"\nCurrent Active DSP Graph:\n{json.dumps(current_graph.model_dump(), indent=2)}"
        else:
            mode_instruction = (
                "MODE: RIG ASSEMBLY (NEW RIG).\n"
                "Pick one fitting TONE3000 Amp (MANDATORY). At your discretion, pick 0-2 complementary Stompbox pedals "
                "and 1 Space (reverb/room/cab). STRICT RULE: Use the authentic default_preset_knobs for each gear item off rip!"
            )
            current_state_summary = ""

        user_content = (
            f"{mode_instruction}\n\n"
            f"User Prompt / Live Voice Command: '{command}'\n"
            f"{current_state_summary}\n\n"
            f"Available TONE3000 Captures for Inspiration:\n{json.dumps(gear_inventory, indent=2)}\n"
            f"Available Acoustic & Tone Tags: {json.dumps(all_tags[:20])}\n\n"
            "Respond with a valid JSON object matching the apply_dsp_graph schema (including intent, tuning_action, changes_summary, emotional_tags, global_gain_db, and nodes)."
        )

        messages = [{"role": "system", "content": GUITAR_AI_SYSTEM_PROMPT}]

        if conversation_history:
            for item in conversation_history[-6:]:
                messages.append(item)

        messages.append({"role": "user", "content": user_content})

        try:
            # We call the model with temperature=0.7 for creative inspiration and ample max_tokens
            response = self.openai_client.chat.completions.create(
                model=self.model,
                messages=messages,
                temperature=0.7,
                max_tokens=1500
            )

            msg = response.choices[0].message
            parsed_args = None

            # 1. Try parsing tool calls if returned
            if getattr(msg, "tool_calls", None) and len(msg.tool_calls) > 0:
                try:
                    parsed_args = json.loads(msg.tool_calls[0].function.arguments)
                except Exception:
                    pass

            # 2. Try parsing JSON directly from message content or reasoning content
            if not parsed_args and msg.content:
                parsed_args = _extract_json_from_text(msg.content)

            if not parsed_args and hasattr(msg, "reasoning_content") and msg.reasoning_content:
                parsed_args = _extract_json_from_text(msg.reasoning_content)

            if parsed_args and isinstance(parsed_args, dict) and "nodes" in parsed_args:
                nodes = []
                top_cap = matching_captures[0] if matching_captures else None

                gear_fallbacks = {
                    "Distortion": ("TS9 Tube Screamer", "Ibanez", "Classic Mid-Hump Overdrive"),
                    "Delay": ("RE-201 Space Echo", "Roland", "Stereo Tape Flutter 350ms"),
                    "Reverb": ("Lexicon 224 Digital Reverb", "Lexicon", "Cathedral & Concert Hall Plate"),
                    "Chorus": ("CE-2 Chorus Ensemble", "Boss", "Analog Bucket-Brigade Mod"),
                    "Phaser": ("Phase 90", "MXR", "Script 4-Stage Phaser"),
                    "Compressor": ("LA-2A Optical Compressor", "Teletronix", "Smooth Opto Peak Limiter"),
                    "PitchShift": ("Whammy V", "DigiTech", "Octave & Pitch Shifter"),
                    "Bitcrush": ("Bitmap 2", "Red Panda", "Sample Reducer & Bitcrusher")
                }

                for n_data in parsed_args.get("nodes", []):
                    ntype = n_data["node_type"]
                    g_model = n_data.get("gear_model")
                    g_make = n_data.get("gear_make")
                    p_name = n_data.get("patch_name")
                    t_id = n_data.get("tone_id")

                    if ntype == "Amp":
                        if not g_model or not p_name:
                            if top_cap:
                                g_model = g_model or top_cap.amp_model
                                p_name = p_name or top_cap.title
                                t_id = t_id or top_cap.tone_id
                                g_make = g_make or getattr(top_cap, "make", "TONE3000")
                            else:
                                g_model = g_model or "Marshall JCM800 2203"
                                g_make = g_make or "Marshall"
                                p_name = p_name or "1981 Cranked Lead"
                    elif ntype in gear_fallbacks:
                        def_model, def_make, def_patch = gear_fallbacks[ntype]
                        g_model = g_model or def_model
                        g_make = g_make or def_make
                        p_name = p_name or def_patch

                    g_type = n_data.get("gear_type")
                    if not g_type:
                        if ntype == "Amp":
                            g_type = "Amp"
                        elif ntype in ["Reverb", "PeakFilter"] and "cab" in (p_name or "").lower():
                            g_type = "Space"
                        elif ntype == "Reverb":
                            g_type = "Space"
                        else:
                            g_type = "Stompbox"

                    n_obj = DSPNodeConfig(
                        node_type=ntype,
                        gear_type=g_type,
                        enabled=n_data.get("enabled", True),
                        display_name=n_data.get("display_name"),
                        gear_model=g_model,
                        gear_make=g_make,
                        patch_name=p_name,
                        tone_id=t_id,
                        params=n_data.get("params", {})
                    )
                    populate_full_node_params(n_obj)
                    nodes.append(n_obj)

                # Filter out redundant filler tags if returned
                raw_tags = parsed_args.get("emotional_tags", [])
                clean_tags = [t for t in raw_tags if t.lower() not in ["ai-synthesized", "clean-amp"]]

                # Enforce Amp stage if missing on new board
                has_amp = any(n.node_type in ["Amp", "Distortion"] for n in nodes)
                if not has_amp and not has_active_board:
                    amp_knobs = top_cap.default_knobs if top_cap else {}
                    amp_node = DSPNodeConfig(
                        node_type="Amp",
                        enabled=True,
                        display_name=top_cap.title if top_cap else "Marshall JCM800 2203 Cranked",
                        gear_model=top_cap.amp_model if top_cap else "Marshall JCM800 2203",
                        gear_make=getattr(top_cap, "make", "Marshall") if top_cap else "Marshall",
                        patch_name=top_cap.title if top_cap else "1981 Cranked Lead High Gain",
                        tone_id=top_cap.tone_id if top_cap else "t3k-jcm800-80s",
                        params={
                            "drive_db": float(amp_knobs.get("gain", 0.5) * 30.0 - 6.0),
                            "bass_db": float((amp_knobs.get("bass", 0.5) - 0.5) * 12.0),
                            "mid_db": float((amp_knobs.get("mid", 0.5) - 0.5) * 12.0),
                            "treble_db": float((amp_knobs.get("treble", 0.5) - 0.5) * 12.0)
                        }
                    )
                    populate_full_node_params(amp_node)
                    nodes.insert(0, amp_node)

                # Enforce space node if building new board
                has_space = any(n.node_type == "Reverb" for n in nodes)
                if not has_space and not has_active_board:
                    space_node = DSPNodeConfig(
                        node_type="Reverb",
                        enabled=True,
                        display_name="Concert Space Reverb",
                        gear_model="Lexicon 224 Digital Reverb",
                        gear_make="Lexicon",
                        patch_name="Concert Hall Plate Decay",
                        params={"room_size": 0.65, "wet_level": 0.35, "damping": 0.35}
                    )
                    populate_full_node_params(space_node)
                    nodes.append(space_node)

                selected_gear_summary = []
                for n in nodes:
                    selected_gear_summary.append({
                        "node_type": n.node_type,
                        "display_name": n.display_name or n.node_type,
                        "gear_model": n.gear_model or n.node_type,
                        "gear_make": n.gear_make or "",
                        "patch_name": n.patch_name or "",
                        "tone_id": n.tone_id or ""
                    })

                gear_rationale = parsed_args.get("gear_selection_rationale")
                if not gear_rationale:
                    amp_n = next((n for n in nodes if n.node_type == "Amp"), None)
                    amp_str = f"{amp_n.gear_make} {amp_n.gear_model} [{amp_n.patch_name}]" if amp_n else "Curated Amp Capture"
                    fx_str = ", ".join([f"{n.gear_model or n.node_type}" for n in nodes if n.node_type != "Amp"])
                    gear_rationale = f"Curated {amp_str} paired with {fx_str} for harmonic richness and authentic analog texture."

                graph = DSPGraphConfig(
                    intent=parsed_args.get("intent", f"Prompt: {command}"),
                    gear_selection_rationale=gear_rationale,
                    emotional_tags=clean_tags if clean_tags else (matching_captures[0].tags if matching_captures else ["tone3000"]),
                    global_gain_db=float(parsed_args.get("global_gain_db", 0.0)),
                    target_instrument=instrument_context,
                    nodes=nodes,
                    selected_gear_summary=selected_gear_summary
                )
                elapsed_ms = (time.perf_counter() - start_time) * 1000.0
                tuning_action = parsed_args.get("tuning_action", "live_tuned" if has_active_board else "board_created")
                changes_summary = parsed_args.get("changes_summary", "Board synthesized from inspiration.")

                return NebiusSoundEngineerResult(
                    graph,
                    elapsed_ms,
                    tuning_action=tuning_action,
                    changes_summary=changes_summary
                )
            else:
                print(f"[Nebius Client] Warning: Response was not valid JSON. Using offline TONE3000 fallback.")
                graph = tone3000_search_fallback(command, instrument=instrument_context, current_graph=current_graph)
                elapsed_ms = (time.perf_counter() - start_time) * 1000.0
                return NebiusSoundEngineerResult(graph, elapsed_ms, tuning_action="board_created", changes_summary="Fallback board created.")

        except Exception as e:
            print(f"[Nebius Client] API call notice ({e}). Utilizing dynamic TONE3000 search fallback.")
            graph = tone3000_search_fallback(command, instrument=instrument_context, current_graph=current_graph)
            elapsed_ms = (time.perf_counter() - start_time) * 1000.0
            return NebiusSoundEngineerResult(graph, elapsed_ms, tuning_action="live_tuned" if has_active_board else "board_created", changes_summary=f"Offline fallback: {command}")

    def suggest_gear_additions(
        self,
        current_graph: Optional[DSPGraphConfig] = None,
        vibe_prompt: Optional[str] = None
    ) -> List[Dict[str, Any]]:
        """
        Uses Nebius AI to analyze current pedalboard chain and recommend 2-3 specific
        gear additions or tweaks that elevate the tone.
        """
        active_nodes = current_graph.nodes if (current_graph and current_graph.nodes) else []
        node_summary = [f"{n.node_type}: {n.gear_model or n.display_name}" for n in active_nodes]

        prompt = (
            f"You are Conductor AI, gear curator for electric guitar.\n"
            f"Current Active Pedalboard Signal Chain:\n"
            f"{json.dumps(node_summary, indent=2) if node_summary else '(No pedals active)'}\n\n"
            f"User Vibe / Goal: '{vibe_prompt or 'suggest complementary pedals or tweaks'}'\n\n"
            f"Suggest 2 or 3 specific stompbox pedals or amps that would elevate this signal chain.\n"
            f"Return a JSON object with this exact structure:\n"
            f"{{\n"
            f'  "suggestions": [\n'
            f'    {{\n'
            f'      "node_type": "Chorus",\n'
            f'      "display_name": "Stereo Analog Chorus",\n'
            f'      "gear_model": "Boss CE-2 Chorus",\n'
            f'      "gear_make": "Boss",\n'
            f'      "patch_name": "Analog Bucket-Brigade Mod",\n'
            f'      "params": {{"rate_hz": 1.1, "depth": 0.45, "mix": 0.4}},\n'
            f'      "rationale": "Adds lush spatial width and 80s shimmer before your space echo."\n'
            f'    }}\n'
            f'  ]\n'
            f"}}"
        )

        if self.openai_client and self.api_key:
            try:
                response = self.openai_client.chat.completions.create(
                    model=self.model,
                    messages=[
                        {"role": "system", "content": "You are Conductor AI, an expert guitar gear curator."},
                        {"role": "user", "content": prompt}
                    ],
                    temperature=0.7,
                    max_tokens=800
                )
                raw_text = response.choices[0].message.content or ""
                parsed = _extract_json_from_text(raw_text)
                if parsed and isinstance(parsed, dict) and "suggestions" in parsed and isinstance(parsed["suggestions"], list):
                    normalized = []
                    for s in parsed["suggestions"][:3]:
                        if isinstance(s, dict):
                            norm = dict(s)
                            norm["model"] = s.get("gear_model") or s.get("model") or s.get("display_name", "")
                            norm["make"] = s.get("gear_make") or s.get("make", "")
                            norm["why_it_fits"] = s.get("rationale") or s.get("why_it_fits", "")
                            norm["default_params"] = s.get("params") or s.get("default_params", {})
                            norm["category"] = s.get("category") or s.get("node_type", "").lower()
                            normalized.append(norm)
                    if normalized:
                        return normalized
            except Exception as e:
                print(f"[Nebius Client] Suggestion API notice: {e}")

        # Smart contextual fallbacks if offline or API error
        suggestions = []
        has_delay = any(n.node_type == "Delay" for n in active_nodes)
        has_reverb = any(n.node_type == "Reverb" for n in active_nodes)
        has_chorus = any(n.node_type in ["Chorus", "Phaser"] for n in active_nodes)
        has_drive = any(n.node_type == "Distortion" for n in active_nodes)

        if not has_drive:
            suggestions.append({
                "node_type": "Distortion",
                "display_name": "Ibanez TS9 Tube Screamer",
                "gear_model": "TS9 Tube Screamer",
                "gear_make": "Ibanez",
                "patch_name": "Classic Mid-Hump Overdrive",
                "params": {"drive_db": 16.0},
                "rationale": "Boosts picking dynamics and tightens low end into the amp."
            })
        if not has_delay:
            suggestions.append({
                "node_type": "Delay",
                "display_name": "Roland RE-201 Space Echo",
                "gear_model": "RE-201 Space Echo",
                "gear_make": "Roland",
                "patch_name": "Tape Echo & Flutter",
                "params": {"delay_seconds": 0.38, "feedback": 0.52, "mix": 0.35},
                "rationale": "Adds vintage tape repeats that create authentic three-dimensional depth."
            })
        if not has_chorus:
            suggestions.append({
                "node_type": "Chorus",
                "display_name": "Boss CE-2 Chorus Ensemble",
                "gear_model": "CE-2 Chorus",
                "gear_make": "Boss",
                "patch_name": "Analog Bucket-Brigade Mod",
                "params": {"rate_hz": 1.1, "depth": 0.45, "mix": 0.4},
                "rationale": "Adds lush 80s shimmer and width to chords and lead lines."
            })
        if not has_reverb:
            suggestions.append({
                "node_type": "Reverb",
                "display_name": "Lexicon 224 Digital Reverb",
                "gear_model": "224 Digital Reverb",
                "gear_make": "Lexicon",
                "patch_name": "Concert Hall Plate",
                "params": {"room_size": 0.85, "damping": 0.3, "wet_level": 0.4, "dry_level": 0.8, "width": 1.0},
                "rationale": "Creates an expansive studio acoustic tail behind every phrase."
            })

        # Normalize suggestion dictionaries with convenient aliases
        normalized_suggestions = []
        for s in suggestions[:3]:
            norm = dict(s)
            norm["model"] = s.get("gear_model") or s.get("display_name", "")
            norm["make"] = s.get("gear_make", "")
            norm["why_it_fits"] = s.get("rationale") or s.get("why_it_fits", "")
            norm["default_params"] = s.get("params") or s.get("default_params", {})
            norm["category"] = s.get("category") or s.get("node_type", "").lower()
            normalized_suggestions.append(norm)

        return normalized_suggestions

