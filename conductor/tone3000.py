"""
TONE3000 API Client & Neural Amp Modeler (NAM) Capture Library Integration.
Enables searching and retrieving 700,000+ guitar amp captures, pedals, and IRs from TONE3000.
"""

import os
import json
import urllib.request
import urllib.parse
from typing import Dict, List, Optional, Any
from pydantic import BaseModel, Field

from .config import config


class Tone3000Capture(BaseModel):
    """Metadata representing a Neural Amp Modeler (NAM) capture on TONE3000."""
    tone_id: str = Field(..., description="Unique TONE3000 capture identifier")
    title: str = Field(..., description="Capture name (e.g., '1980 JCM800 Lead High Gain')")
    author: str = Field(default="Community", description="Creator of the capture")
    gear_type: str = Field(default="Amp", description="Amp, Stompbox, Full Rig, or Cabinet")
    amp_model: str = Field(default="Marshall JCM800", description="Original physical amplifier modeled")
    gain_level: str = Field(default="High Gain", description="Clean, Edge of Breakup, Crunch, High Gain")
    download_url: Optional[str] = None
    tags: List[str] = Field(default_factory=list)
    default_knobs: Dict[str, float] = Field(default_factory=dict)


# Local fallback database of popular pre-cached TONE3000 guitar captures (Amps, Stompboxes, Spaces)
FEATURED_TONE3000_CAPTURES: List[Dict[str, Any]] = [
    # --- AMPS (Mandatory Core) ---
    {
        "tone_id": "t3k-jcm800-80s",
        "title": "Marshall JCM800 2203 (1981 Cranked Lead)",
        "author": "Marshall",
        "gear_type": "Amp",
        "amp_model": "Marshall JCM800",
        "gain_level": "High Gain",
        "tags": ["marshall", "jcm800", "hard rock", "crunchy", "80s lead", "heavy"],
        "default_knobs": {"gain": 0.75, "bass": 0.5, "mid": 0.65, "treble": 0.7, "presence": 0.6}
    },
    {
        "tone_id": "t3k-fender-twin-clean",
        "title": "Fender Twin Reverb '65 Blackface Pristine Clean",
        "author": "Fender",
        "gear_type": "Amp",
        "amp_model": "Fender Twin Reverb",
        "gain_level": "Clean",
        "tags": ["fender", "twin reverb", "clean", "glassy", "airy", "surf"],
        "default_knobs": {"gain": 0.25, "bass": 0.6, "mid": 0.45, "treble": 0.8, "presence": 0.7}
    },
    {
        "tone_id": "t3k-recto-metal",
        "title": "Mesa/Boogie Dual Rectifier Solo (Red Channel Heavy)",
        "author": "Mesa Boogie",
        "gear_type": "Amp",
        "amp_model": "Mesa Boogie Dual Rectifier",
        "gain_level": "High Gain",
        "tags": ["mesa", "boogie", "rectifier", "metal", "scooped", "heavy", "chug"],
        "default_knobs": {"gain": 0.85, "bass": 0.7, "mid": 0.35, "treble": 0.75, "presence": 0.65}
    },
    {
        "tone_id": "t3k-plex-crunch",
        "title": "1968 Marshall Super Lead Plexi 100W Overdrive",
        "author": "Marshall",
        "gear_type": "Amp",
        "amp_model": "Marshall Plexi",
        "gain_level": "Crunch",
        "tags": ["plexi", "marshall", "vintage", "warm tube", "rock", "crunch"],
        "default_knobs": {"gain": 0.65, "bass": 0.55, "mid": 0.7, "treble": 0.6, "presence": 0.55}
    },
    {
        "tone_id": "t3k-vox-ac30-chime",
        "title": "Vox AC30 Top Boost 1964 Chimey Breakup",
        "author": "Vox",
        "gear_type": "Amp",
        "amp_model": "Vox AC30",
        "gain_level": "Edge of Breakup",
        "tags": ["vox", "ac30", "chime", "indie", "britpop", "bright"],
        "default_knobs": {"gain": 0.45, "bass": 0.45, "mid": 0.6, "treble": 0.85, "presence": 0.75}
    },
    {
        "tone_id": "t3k-soldano-slo100",
        "title": "Soldano SLO-100 Super Lead Overdrive Liquid Lead",
        "author": "Soldano",
        "gear_type": "Amp",
        "amp_model": "Soldano SLO-100",
        "gain_level": "High Gain",
        "tags": ["soldano", "slo100", "liquid lead", "singing sustain", "80s solo"],
        "default_knobs": {"gain": 0.8, "bass": 0.6, "mid": 0.75, "treble": 0.65, "presence": 0.7}
    },
    {
        "tone_id": "t3k-evh-5150iii",
        "title": "EVH 5150 III Stealth Red Channel Aggressive",
        "author": "EVH",
        "gear_type": "Amp",
        "amp_model": "EVH 5150",
        "gain_level": "High Gain",
        "tags": ["evh", "5150", "chug", "tight", "metalcore", "brown sound"],
        "default_knobs": {"gain": 0.88, "bass": 0.65, "mid": 0.55, "treble": 0.7, "presence": 0.8}
    },
    {
        "tone_id": "t3k-dumble-ods",
        "title": "Dumble Overdrive Special Smooth Fusion Lead",
        "author": "Dumble",
        "gear_type": "Amp",
        "amp_model": "Dumble Overdrive Special",
        "gain_level": "Crunch",
        "tags": ["dumble", "ods", "smooth", "fusion", "warm tube", "boutique"],
        "default_knobs": {"gain": 0.55, "bass": 0.5, "mid": 0.8, "treble": 0.55, "presence": 0.45}
    },
    {
        "tone_id": "t3k-roland-jc120",
        "title": "Roland JC-120 Jazz Chorus Pristine Stereo Clean",
        "author": "Roland",
        "gear_type": "Amp",
        "amp_model": "Roland JC-120",
        "gain_level": "Clean",
        "tags": ["roland", "jc120", "jazz chorus", "pristine", "ambient", "underwater", "cathedral", "space", "shimmer"],
        "default_knobs": {"gain": 0.15, "bass": 0.5, "mid": 0.5, "treble": 0.85, "presence": 0.8}
    },
    {
        "tone_id": "t3k-orange-rockerverb",
        "title": "Orange Rockerverb 100 MKIII Fuzzy British Crunch",
        "author": "Orange",
        "gear_type": "Amp",
        "amp_model": "Orange Rockerverb 100",
        "gain_level": "High Gain",
        "tags": ["orange", "rockerverb", "doom", "sludge", "fuzz", "thick", "heavy"],
        "default_knobs": {"gain": 0.82, "bass": 0.8, "mid": 0.4, "treble": 0.6, "presence": 0.5}
    },
    {
        "tone_id": "t3k-bogner-ecstasy",
        "title": "Bogner Ecstasy 101B Blue Channel Singing Lead",
        "author": "Bogner",
        "gear_type": "Amp",
        "amp_model": "Bogner Ecstasy 101B",
        "gain_level": "Crunch",
        "tags": ["bogner", "ecstasy", "liquid lead", "boutique", "singing sustain"],
        "default_knobs": {"gain": 0.68, "bass": 0.6, "mid": 0.7, "treble": 0.65, "presence": 0.6}
    },

    # --- STOMPBOXES / PEDALS (At AI Discretion or User Selection) ---
    {
        "tone_id": "t3k-ts9-tube-screamer",
        "title": "Ibanez TS9 Tube Screamer",
        "author": "Ibanez",
        "gear_type": "Stompbox",
        "amp_model": "TS9 Tube Screamer",
        "gain_level": "Crunch",
        "tags": ["overdrive", "boost", "tube screamer", "screamer", "mid-hump", "crunch", "tight", "lead", "blues"],
        "default_knobs": {"drive_db": 12.0, "tone": 0.6, "level_db": 0.0}
    },
    {
        "tone_id": "t3k-klon-centaur",
        "title": "Klon Centaur Professional Overdrive",
        "author": "Klon",
        "gear_type": "Stompbox",
        "amp_model": "Klon Centaur",
        "gain_level": "Crunch",
        "tags": ["klon", "centaur", "transparent", "clean boost", "overdrive", "warm", "dynamic", "boutique"],
        "default_knobs": {"drive_db": 8.0, "treble": 0.5, "level_db": 2.0}
    },
    {
        "tone_id": "t3k-big-muff",
        "title": "Electro-Harmonix Big Muff Pi (NYC)",
        "author": "Electro-Harmonix",
        "gear_type": "Stompbox",
        "amp_model": "Big Muff Pi",
        "gain_level": "High Gain",
        "tags": ["fuzz", "big muff", "muff", "thick", "heavy", "grunge", "doom", "stoner", "sustain"],
        "default_knobs": {"drive_db": 22.0, "tone": 0.5, "level_db": 0.0}
    },
    {
        "tone_id": "t3k-proco-rat",
        "title": "ProCo Rat 2 Distortion",
        "author": "ProCo",
        "gear_type": "Stompbox",
        "amp_model": "Rat 2",
        "gain_level": "High Gain",
        "tags": ["rat", "distortion", "hard clipping", "punk", "80s", "metal", "gritty"],
        "default_knobs": {"drive_db": 18.0, "filter": 0.55, "level_db": 0.0}
    },
    {
        "tone_id": "t3k-boss-ce2",
        "title": "Boss CE-2 Chorus Ensemble",
        "author": "Boss",
        "gear_type": "Stompbox",
        "amp_model": "CE-2 Chorus",
        "gain_level": "Clean",
        "tags": ["chorus", "analog", "shimmer", "80s", "lush", "clean", "glassy", "dreamy"],
        "default_knobs": {"rate_hz": 1.2, "depth": 0.55, "mix": 0.4}
    },
    {
        "tone_id": "t3k-mxr-phase90",
        "title": "MXR Phase 90 Script Logo",
        "author": "MXR",
        "gear_type": "Stompbox",
        "amp_model": "Phase 90",
        "gain_level": "Clean",
        "tags": ["phaser", "swirl", "van halen", "psychedelic", "modulation", "sweeping"],
        "default_knobs": {"rate_hz": 0.8, "depth": 0.6, "mix": 0.5}
    },
    {
        "tone_id": "t3k-space-echo",
        "title": "Roland RE-201 Space Echo Tape Delay",
        "author": "Roland",
        "gear_type": "Stompbox",
        "amp_model": "RE-201 Space Echo",
        "gain_level": "Clean",
        "tags": ["delay", "echo", "tape echo", "repeats", "flutter", "space", "ambient", "slapback"],
        "default_knobs": {"delay_seconds": 0.38, "feedback": 0.45, "mix": 0.35}
    },
    {
        "tone_id": "t3k-dyna-comp",
        "title": "MXR Dyna Comp Compressor",
        "author": "MXR",
        "gear_type": "Stompbox",
        "amp_model": "Dyna Comp",
        "gain_level": "Clean",
        "tags": ["compressor", "percussive", "sustain", "spank", "funk", "country", "tight"],
        "default_knobs": {"threshold_db": -20.0, "ratio": 4.0, "attack_ms": 10.0}
    },
    {
        "tone_id": "t3k-isp-decimator",
        "title": "ISP Technologies Decimator II Noise Gate",
        "author": "ISP Technologies",
        "gear_type": "Stompbox",
        "amp_model": "Decimator II",
        "gain_level": "Clean",
        "tags": ["noise gate", "gate", "chug", "metal", "tight", "silent", "high gain"],
        "default_knobs": {"threshold_db": -42.0, "ratio": 10.0, "release_ms": 30.0}
    },

    # --- SPACES & REVERBS & CABINETS (At AI Discretion or User Selection) ---
    {
        "tone_id": "t3k-lexicon-224",
        "title": "Lexicon 224 Digital Concert Hall & Plate",
        "author": "Lexicon",
        "gear_type": "Space",
        "amp_model": "224 Digital Reverb",
        "gain_level": "Clean",
        "tags": ["reverb", "plate", "hall", "cathedral", "space", "ambient", "underwater", "vast", "decay"],
        "default_knobs": {"room_size": 0.85, "damping": 0.3, "wet_level": 0.35, "dry_level": 0.85, "width": 1.0}
    },
    {
        "tone_id": "t3k-fender-6g15",
        "title": "Fender 6G15 Vintage Tube Spring Reverb Unit",
        "author": "Fender",
        "gear_type": "Space",
        "amp_model": "6G15 Spring Reverb",
        "gain_level": "Clean",
        "tags": ["spring", "surf", "vintage", "twang", "drip", "warm", "reverb"],
        "default_knobs": {"room_size": 0.55, "damping": 0.2, "wet_level": 0.4, "dry_level": 0.85, "width": 0.8}
    },
    {
        "tone_id": "t3k-abbey-road-room",
        "title": "Abbey Road Studio Two Live Acoustic Room",
        "author": "Abbey Road",
        "gear_type": "Space",
        "amp_model": "Studio Two Live Room",
        "gain_level": "Clean",
        "tags": ["room", "studio", "acoustic", "natural", "organic", "space"],
        "default_knobs": {"room_size": 0.35, "damping": 0.5, "wet_level": 0.25, "dry_level": 0.9, "width": 0.9}
    },
    {
        "tone_id": "t3k-marshall-1960a",
        "title": "Marshall 1960A 4x12 Greenback Cab Space",
        "author": "Marshall",
        "gear_type": "Space",
        "amp_model": "1960A 4x12 Greenback",
        "gain_level": "Crunch",
        "tags": ["cabinet", "4x12", "greenback", "british", "rock", "punchy"],
        "default_knobs": {"room_size": 0.25, "damping": 0.6, "wet_level": 0.2, "dry_level": 0.9, "width": 0.7}
    },
    {
        "tone_id": "t3k-mesa-recto-cab",
        "title": "Mesa Traditional 4x12 Celestion V30 Space",
        "author": "Mesa Boogie",
        "gear_type": "Space",
        "amp_model": "Mesa 4x12 V30",
        "gain_level": "High Gain",
        "tags": ["cabinet", "4x12", "v30", "metal", "tight", "heavy"],
        "default_knobs": {"room_size": 0.3, "damping": 0.55, "wet_level": 0.2, "dry_level": 0.9, "width": 0.8}
    }
]


class Tone3000Client:
    """
    Client for searching, downloading, and loading Neural Amp Modeler (NAM)
    captures via the TONE3000 API.
    """

    def __init__(self, api_key: Optional[str] = None):
        self.api_key = api_key or config.tone3000_api_key or os.getenv("TONE3000_API_KEY", "")
        self.base_url = config.tone3000_base_url or "https://www.tone3000.com/api/v1"

    def search_tones(self, query: str = "", gear_type: Optional[str] = None) -> List[Tone3000Capture]:
        """
        Search TONE3000 capture library for guitar amp/pedal/space profiles matching query and optional gear_type.
        """
        q_lower = query.lower().strip() if query else ""

        # Try live API request if API key is configured
        if self.api_key and q_lower:
            try:
                url = f"{self.base_url}/tones/search?query={urllib.parse.quote(query)}"
                req = urllib.request.Request(url, headers={"Authorization": f"Bearer {self.api_key}"})
                with urllib.request.urlopen(req, timeout=4.0) as response:
                    data = json.loads(response.read().decode())
                    results = []
                    for item in data.get("data", []):
                        tags = [t.get("name", "") for t in item.get("tags", []) if isinstance(t, dict)]
                        makes = [m.get("name", "") for m in item.get("makes", []) if isinstance(m, dict)]
                        amp_model = makes[0] if makes else item.get("gear", "Amp")
                        raw_gear = item.get("gear", "Amp").capitalize()
                        if raw_gear in ["Stompbox", "Pedal"]:
                            g_type = "Stompbox"
                        elif raw_gear in ["Cabinet", "Ir", "Reverb", "Space"]:
                            g_type = "Space"
                        else:
                            g_type = "Amp"

                        if gear_type and g_type.lower() != gear_type.lower():
                            continue

                        is_high_gain = any("higain" in t.lower() or "crunch" in t.lower() or "lead" in t.lower() or "metal" in t.lower() for t in tags)
                        results.append(Tone3000Capture(
                            tone_id=str(item.get("id")),
                            title=item.get("title") or item.get("name", "TONE3000 Capture"),
                            author=item.get("user", {}).get("username", "Community") if isinstance(item.get("user"), dict) else "Community",
                            gear_type=g_type,
                            amp_model=amp_model,
                            gain_level="High Gain" if is_high_gain else "Clean",
                            download_url=item.get("url"),
                            tags=tags,
                            default_knobs={
                                "gain": 0.75 if is_high_gain else 0.35,
                                "bass": 0.55,
                                "mid": 0.6,
                                "treble": 0.65,
                                "presence": 0.55
                            }
                        ))
                    if results:
                        return results
            except Exception as e:
                print(f"[TONE3000 API] Live search notice: ({e}). Using TONE3000 featured library index.")

        # Match against pre-cached TONE3000 index
        candidate_list = FEATURED_TONE3000_CAPTURES
        if gear_type:
            candidate_list = [c for c in candidate_list if c["gear_type"].lower() == gear_type.lower()]

        if not q_lower:
            return [
                Tone3000Capture(
                    tone_id=m["tone_id"],
                    title=m["title"],
                    author=m["author"],
                    gear_type=m["gear_type"],
                    amp_model=m["amp_model"],
                    gain_level=m["gain_level"],
                    tags=m["tags"],
                    default_knobs=m.get("default_knobs", {})
                )
                for m in candidate_list
            ]

        matched = []
        for item in candidate_list:
            score = 0
            for tag in item["tags"]:
                if tag in q_lower:
                    score += 2
            if item["amp_model"].lower() in q_lower or item["title"].lower() in q_lower:
                score += 3
            if q_lower in item["title"].lower() or q_lower in item["amp_model"].lower():
                score += 1

            if score > 0:
                matched.append((score, item))

        if matched:
            matched.sort(key=lambda x: x[0], reverse=True)
            return [
                Tone3000Capture(
                    tone_id=m[1]["tone_id"],
                    title=m[1]["title"],
                    author=m[1]["author"],
                    gear_type=m[1]["gear_type"],
                    amp_model=m[1]["amp_model"],
                    gain_level=m[1]["gain_level"],
                    tags=m[1]["tags"],
                    default_knobs=m[1].get("default_knobs", {})
                )
                for m in matched
            ]

        # Default fallback to first item in candidate list
        default_item = candidate_list[0] if candidate_list else FEATURED_TONE3000_CAPTURES[0]
        return [
            Tone3000Capture(
                tone_id=default_item["tone_id"],
                title=default_item["title"],
                author=default_item["author"],
                gear_type=default_item["gear_type"],
                amp_model=default_item["amp_model"],
                gain_level=default_item["gain_level"],
                tags=default_item["tags"],
                default_knobs=default_item.get("default_knobs", {})
            )
        ]

    def get_all_tags(self) -> List[str]:
        """Returns sorted list of all unique acoustic and gear tags in TONE3000 catalog."""
        tag_set = set()
        for item in FEATURED_TONE3000_CAPTURES:
            for t in item.get("tags", []):
                tag_set.add(t)
        return sorted(list(tag_set))


def tone3000_search_fallback(
    command: str,
    instrument: str = "guitar",
    current_graph: Optional[Any] = None
) -> Any:
    """
    Dynamic gear search and tuning engine:
    - If current_graph exists and command is conversational tuning/swapping, surgically mutates the active board.
    - Otherwise, builds a new pedalboard setup matching the prompt from the TONE3000 capture catalog.
    """
    from .schemas import DSPGraphConfig, DSPNodeConfig
    from .dsp_engine import populate_full_node_params

    cmd_lower = command.lower()

    # Direct DI / Bypass request
    if any(w in cmd_lower for w in ["direct", "dry di", "bypass", "clean di", "raw guitar"]):
        return DSPGraphConfig(
            intent="Direct DI (Clean Bypass)",
            gear_selection_rationale="Audio routing direct to monitor/output without amplifier coloration.",
            emotional_tags=["direct", "dry", "di"],
            global_gain_db=0.0,
            target_instrument=instrument,
            is_direct=True,
            nodes=[]
        )

    # Conversational Live Tuning on existing graph
    if current_graph and getattr(current_graph, "nodes", None) and len(current_graph.nodes) > 0:
        is_fresh_request = any(w in cmd_lower for w in ["build new", "reset", "start over", "brand new"])
        tuning_keywords = [
            "more", "less", "turn up", "turn down", "cut", "boost", "darker", "brighter",
            "heavier", "cleaner", "gain", "drive", "treble", "bass", "mid", "presence", "volume",
            "delay", "reverb", "echo", "chorus", "mix", "feedback", "rate", "depth"
        ]
        is_tuning = not is_fresh_request and any(k in cmd_lower for k in tuning_keywords)

        if is_tuning:
            # Clone existing graph nodes surgically
            new_nodes = [
                DSPNodeConfig(
                    node_type=n.node_type,
                    gear_type=getattr(n, "gear_type", "Amp"),
                    enabled=n.enabled,
                    display_name=n.display_name,
                    gear_model=n.gear_model,
                    gear_make=n.gear_make,
                    patch_name=n.patch_name,
                    tone_id=n.tone_id,
                    params=dict(n.params)
                ) for n in current_graph.nodes
            ]

            # Adjust parameters across nodes
            for n in new_nodes:
                p = n.params
                nt = n.node_type
                if nt == "Amp":
                    if "more gain" in cmd_lower or "more drive" in cmd_lower or "heavier" in cmd_lower:
                        curr_gain = float(p.get("gain_db", p.get("drive_db", 12.0)))
                        p["gain_db"] = min(curr_gain + 6.0, 36.0)
                        p["drive_db"] = p["gain_db"]
                    elif "less gain" in cmd_lower or "cleaner" in cmd_lower:
                        curr_gain = float(p.get("gain_db", p.get("drive_db", 12.0)))
                        p["gain_db"] = max(curr_gain - 6.0, -12.0)
                        p["drive_db"] = p["gain_db"]

                    if "brighter" in cmd_lower or "more treble" in cmd_lower or "more bite" in cmd_lower:
                        p["treble_db"] = min(float(p.get("treble_db", 0.0)) + 3.0, 12.0)
                        p["presence_db"] = min(float(p.get("presence_db", 0.0)) + 3.0, 12.0)
                    elif "darker" in cmd_lower or "less treble" in cmd_lower or "cut treble" in cmd_lower:
                        p["treble_db"] = max(float(p.get("treble_db", 0.0)) - 4.0, -12.0)
                        p["presence_db"] = max(float(p.get("presence_db", 0.0)) - 4.0, -12.0)

                    if "more bass" in cmd_lower or "thicker" in cmd_lower or "fatter" in cmd_lower:
                        p["bass_db"] = min(float(p.get("bass_db", 0.0)) + 3.0, 12.0)
                    elif "less bass" in cmd_lower or "cut low" in cmd_lower:
                        p["bass_db"] = max(float(p.get("bass_db", 0.0)) - 3.0, -12.0)

                    if "scoop" in cmd_lower or "less mid" in cmd_lower:
                        p["mid_db"] = max(float(p.get("mid_db", 0.0)) - 4.0, -12.0)
                    elif "more mid" in cmd_lower or "boost mid" in cmd_lower:
                        p["mid_db"] = min(float(p.get("mid_db", 0.0)) + 3.0, 12.0)

                elif nt in ["Distortion", "Overdrive"]:
                    if "more gain" in cmd_lower or "more drive" in cmd_lower or "more distortion" in cmd_lower:
                        p["drive_db"] = min(float(p.get("drive_db", 12.0)) + 6.0, 36.0)
                    elif "less gain" in cmd_lower or "less drive" in cmd_lower:
                        p["drive_db"] = max(float(p.get("drive_db", 12.0)) - 6.0, 0.0)

                elif nt == "Delay":
                    if "more delay" in cmd_lower or "more echo" in cmd_lower:
                        p["mix"] = min(float(p.get("mix", 0.35)) + 0.15, 0.9)
                        p["feedback"] = min(float(p.get("feedback", 0.45)) + 0.1, 0.85)
                    elif "less delay" in cmd_lower or "cut delay" in cmd_lower or "turn down delay" in cmd_lower:
                        p["mix"] = max(float(p.get("mix", 0.35)) - 0.15, 0.05)

                elif nt == "Reverb":
                    if "more reverb" in cmd_lower or "more space" in cmd_lower or "bigger" in cmd_lower:
                        p["wet_level"] = min(float(p.get("wet_level", 0.35)) + 0.15, 0.95)
                        p["room_size"] = min(float(p.get("room_size", 0.6)) + 0.15, 0.98)
                    elif "less reverb" in cmd_lower or "cut reverb" in cmd_lower or "dryer" in cmd_lower:
                        p["wet_level"] = max(float(p.get("wet_level", 0.35)) - 0.15, 0.05)

                elif nt == "Chorus":
                    if "more chorus" in cmd_lower or "more shimmer" in cmd_lower:
                        p["mix"] = min(float(p.get("mix", 0.4)) + 0.15, 0.9)
                    elif "less chorus" in cmd_lower:
                        p["mix"] = max(float(p.get("mix", 0.4)) - 0.15, 0.05)

            return DSPGraphConfig(
                intent=f"Tuned: '{command}'",
                gear_selection_rationale=current_graph.gear_selection_rationale,
                emotional_tags=current_graph.emotional_tags or ["live_tuned"],
                global_gain_db=current_graph.global_gain_db,
                target_instrument=instrument,
                is_direct=False,
                nodes=new_nodes
            )

    # Initial Rig Selection from TONE3000 Captures
    client = Tone3000Client()

    # 1. Search for Best Amp (Mandatory Core)
    amp_captures = client.search_tones(command, gear_type="Amp")
    top_amp = amp_captures[0] if amp_captures else None
    if not top_amp:
        for c in FEATURED_TONE3000_CAPTURES:
            if c["gear_type"] == "Amp":
                top_amp = Tone3000Capture(**c)
                break

    # STRICT RULE: Use Authentic Default Preset Knobs Off Rip
    amp_knobs = top_amp.default_knobs if top_amp else {"gain": 0.5, "bass": 0.5, "mid": 0.5, "treble": 0.5, "presence": 0.5}
    gain_val = float(amp_knobs.get("gain", 0.5) * 36.0 - 6.0)
    amp_params = {
        "gain_db": gain_val,
        "drive_db": gain_val,
        "bass_db": float((amp_knobs.get("bass", 0.5) - 0.5) * 16.0),
        "mid_db": float((amp_knobs.get("mid", 0.5) - 0.5) * 16.0),
        "treble_db": float((amp_knobs.get("treble", 0.5) - 0.5) * 16.0),
        "presence_db": float((amp_knobs.get("presence", 0.5) - 0.5) * 16.0),
        "volume_db": 0.0
    }
    amp_node = DSPNodeConfig(
        node_type="Amp",
        gear_type="Amp",
        enabled=True,
        display_name=top_amp.title,
        gear_model=top_amp.amp_model,
        gear_make=getattr(top_amp, "author", "TONE3000"),
        patch_name=top_amp.title,
        tone_id=top_amp.tone_id,
        params=amp_params
    )
    populate_full_node_params(amp_node)

    nodes = []

    # 2. Pick Stompbox Pedals (At AI discretion based on prompt keywords)
    stomp_candidates = [c for c in FEATURED_TONE3000_CAPTURES if c["gear_type"] == "Stompbox"]
    matched_pedals = []

    # Check for drive / boost / fuzz / distortion
    if any(w in cmd_lower for w in ["fuzz", "grunge", "doom", "sludge", "muff"]):
        p = next((c for c in stomp_candidates if "big-muff" in c["tone_id"]), None)
        if p: matched_pedals.append((p, "Distortion"))
    elif any(w in cmd_lower for w in ["rat", "punk", "gritty", "hard clipping"]):
        p = next((c for c in stomp_candidates if "rat" in c["tone_id"]), None)
        if p: matched_pedals.append((p, "Distortion"))
    elif any(w in cmd_lower for w in ["screamer", "ts9", "tube screamer", "mid-hump", "tight lead"]):
        p = next((c for c in stomp_candidates if "ts9" in c["tone_id"]), None)
        if p: matched_pedals.append((p, "Distortion"))
    elif any(w in cmd_lower for w in ["klon", "centaur", "transparent", "clean boost"]):
        p = next((c for c in stomp_candidates if "klon" in c["tone_id"]), None)
        if p: matched_pedals.append((p, "Distortion"))
    elif any(w in cmd_lower for w in ["solo", "lead", "metal", "shred", "overdrive", "crunch"]) and "clean" not in cmd_lower:
        p = next((c for c in stomp_candidates if "ts9" in c["tone_id"]), None)
        if p: matched_pedals.append((p, "Distortion"))

    # Check for Noise Gate on heavy/metal prompts
    if any(w in cmd_lower for w in ["chug", "metal", "tight", "5150", "rectifier", "gate", "djent"]):
        p = next((c for c in stomp_candidates if "decimator" in c["tone_id"]), None)
        if p: matched_pedals.insert(0, (p, "NoiseGate"))

    # Check for Compressor on clean/funk/country/spank prompts
    if any(w in cmd_lower for w in ["compressor", "comp", "funk", "spank", "country", "percussive"]):
        p = next((c for c in stomp_candidates if "dyna-comp" in c["tone_id"]), None)
        if p: matched_pedals.insert(0, (p, "Compressor"))

    # Check for Modulation (Chorus / Phaser)
    if any(w in cmd_lower for w in ["chorus", "shimmer", "80s clean", "glassy", "dreamy", "lush"]):
        p = next((c for c in stomp_candidates if "ce2" in c["tone_id"]), None)
        if p: matched_pedals.append((p, "Chorus"))
    elif any(w in cmd_lower for w in ["phase", "phaser", "swirl", "psychedelic", "van halen"]):
        p = next((c for c in stomp_candidates if "phase90" in c["tone_id"]), None)
        if p: matched_pedals.append((p, "Phaser"))

    # Check for Delay (Tape Echo)
    if any(w in cmd_lower for w in ["delay", "echo", "repeats", "tape", "space echo", "flutter", "underwater", "cathedral", "ambient", "shoegaze"]):
        p = next((c for c in stomp_candidates if "space-echo" in c["tone_id"]), None)
        if p: matched_pedals.append((p, "Delay"))

    # Add matched Stompboxes before Amp
    for p_cap, p_node_type in matched_pedals:
        p_node = DSPNodeConfig(
            node_type=p_node_type,
            gear_type="Stompbox",
            enabled=True,
            display_name=p_cap["title"],
            gear_model=p_cap["amp_model"],
            gear_make=p_cap["author"],
            patch_name=p_cap["title"],
            tone_id=p_cap["tone_id"],
            params=dict(p_cap.get("default_knobs", {}))
        )
        populate_full_node_params(p_node)
        nodes.append(p_node)

    # Add Core Amp
    nodes.append(amp_node)

    # 3. Pick Space / Reverb / Cabinet (At AI discretion based on prompt)
    space_candidates = [c for c in FEATURED_TONE3000_CAPTURES if c["gear_type"] == "Space"]
    space_cap = None

    if any(w in cmd_lower for w in ["surf", "spring", "twang", "60s"]):
        space_cap = next((c for c in space_candidates if "6g15" in c["tone_id"]), None)
    elif any(w in cmd_lower for w in ["cathedral", "vast", "ambient", "shoegaze", "underwater", "shimmer", "plate", "hall", "space"]):
        space_cap = next((c for c in space_candidates if "lexicon" in c["tone_id"]), None)
    elif any(w in cmd_lower for w in ["room", "natural", "studio", "organic"]):
        space_cap = next((c for c in space_candidates if "abbey-road" in c["tone_id"]), None)
    else:
        # Default space: subtle studio acoustic room or digital plate
        space_cap = next((c for c in space_candidates if "lexicon" in c["tone_id"]), space_candidates[0] if space_candidates else None)

    if space_cap:
        space_node = DSPNodeConfig(
            node_type="Reverb",
            gear_type="Space",
            enabled=True,
            display_name=space_cap["title"],
            gear_model=space_cap["amp_model"],
            gear_make=space_cap["author"],
            patch_name=space_cap["title"],
            tone_id=space_cap["tone_id"],
            params=dict(space_cap.get("default_knobs", {}))
        )
        populate_full_node_params(space_node)
        nodes.append(space_node)

    tags = top_amp.tags if top_amp else []
    intent_str = f"TONE3000: '{top_amp.title}'"
    pedal_names = [n.display_name for n in nodes if n.gear_type == "Stompbox"]
    space_names = [n.display_name for n in nodes if n.gear_type == "Space"]

    gear_parts = [f"Amp: {amp_node.display_name}"]
    if pedal_names:
        gear_parts.insert(0, f"Pedals: {', '.join(pedal_names)}")
    if space_names:
        gear_parts.append(f"Space: {', '.join(space_names)}")

    gear_rationale = f"Curated {' | '.join(gear_parts)} with stock TONE3000 preset knobs."

    return DSPGraphConfig(
        intent=intent_str,
        gear_selection_rationale=gear_rationale,
        emotional_tags=tags if tags else ["tone3000"],
        global_gain_db=0.0,
        target_instrument=instrument,
        is_direct=False,
        nodes=nodes
    )

# Alias for backward compatibility
semantic_search_dictionary = tone3000_search_fallback

