# Conductor 🎸🪄

> **Autonomous AI Sound Designer & Dynamic DSP Synthesizer**

Conductor is a next-generation AI-powered guitar and audio workstation that bridges neural amp modeling, real-time DSP, and conversational sound design. It translates natural language tone requests (e.g., *"give me an 80s cranked British high-gain sound with a tight low end"*) into authentic Neural Amp Modeler (NAM) profiles and dynamic DSP pedalboard chains in real time.

Conductor ships as both a native **macOS Audio Plugin & Standalone App** (AU / VST3 / Standalone built with JUCE 8 and C++20) and a modular **Python AI Engine & Web Dashboard** (FastAPI, ASR voice control, and low-latency audio streaming).

---

## Key Features

- **Neural Amp Modeling (NAM)**: Powered by `NeuralAmpModelerCore` for ultra-realistic tube amp response and speaker cabinet IR simulation.
- **Conversational Tone Generation**: Prompt-driven tone engineering using Nebius AI Studio (`DeepSeek-V4.1-Flash`) to map musical intent into concrete amp parameters and pedal configs.
- **TONE3000 Catalog Integration**: Automatic searching, matching, and downloading of community and studio NAM captures.
- **Real-Time C++ Audio Plugin (JUCE 8)**:
  - Formats: AudioUnit (AU), VST3, and macOS Standalone application.
  - Interactive gear rack with modular pedal cards and parameter sliders.
  - Low-latency chromatic pitch detection and tuner display.
  - Peak and RMS audio telemetry metering.
  - Integrated live debug console and Siri-style AI dialog overlay.
- **Seamless Live DSP Crossfading**: Glitch-free, equal-power crossfading when swapping profiles or adjusting parameters mid-performance.
- **Dual-Channel Live Hardware Streaming**: Dedicated 2-in / 2-out hardware routing (Channel 0: voice control microphone; Channel 1: dry guitar DI) with instant duplex audio pass-through.
- **Web Dashboard & REST API**: Clean browser UI and FastAPI backend for headless rigs, remote control, and automated audio batch processing.

---

## Repository Layout

```text
Conductor/
├── Source/                  # C++ JUCE 8 Plugin & Standalone Source
│   ├── AI/                  # LLM client, model caching, and Tone3000 catalog loader
│   ├── Audio/               # NAM DSP chain, tuner detector, and audio telemetry
│   ├── UI/                  # LookAndFeel, pedalboard rack, meters, and debug console
│   ├── PluginProcessor.cpp  # Audio processor implementation
│   └── PluginEditor.cpp     # Main plugin GUI implementation
├── conductor/               # Python AI Engine & DSP Backend
│   ├── asr_engine.py        # Voice command transcription (speech-to-text)
│   ├── audio_io.py          # Native hardware duplex audio stream & file processing
│   ├── config.py            # Environment and runtime configuration
│   ├── dsp_engine.py        # Equal-power crossfading DSP pedalboard engine
│   ├── guitar_engine.py     # Intent-to-DSP graph generator
│   ├── nebius_client.py     # Nebius AI Studio LLM client
│   ├── schemas.py           # Pydantic data schemas and graph definitions
│   ├── server.py            # FastAPI REST API and static web server
│   └── tone3000.py          # TONE3000 API client & fallback captures
├── web/                     # Web dashboard interface (HTML / CSS / JS)
├── demo_outputs/            # Pre-rendered audio demonstration results
├── test_input/              # Reference test audio files (clean DI guitar)
├── tests/                   # Pytest automated test suite
├── Libs/                    # Git submodules
│   ├── JUCE/                # JUCE 8 framework
│   └── NeuralAmpModelerCore/# Neural Amp Modeler C++ core engine
├── CMakeLists.txt           # CMake build configuration for C++ targets
├── conductor_core.py        # Python standalone CLI & hardware stream harness
├── demo.py                  # Self-contained audio demonstration script
└── .env.example             # Configuration and API key template
```

---

## Getting Started

### 1. Clone the Repository

Clone with submodules to pull down `JUCE` and `NeuralAmpModelerCore`:

```bash
git clone --recurse-submodules https://github.com/jdawg0507/conductor.git
cd conductor
```

If already cloned without submodules, initialize them:

```bash
git submodule update --init --recursive
```

### 2. Configure Environment Variables

Copy the example environment file and configure your API keys:

```bash
cp .env.example .env
```

Edit `.env` with your credentials:

```ini
NEBIUS_API_KEY=your_nebius_api_key_here
NEBIUS_BASE_URL=https://api.studio.nebius.ai/v1
NEBIUS_LLM_MODEL=deepseek-ai/DeepSeek-V4.1-Flash
TONE3000_API_KEY=your_tone3000_api_key_here
TONE3000_PUBLISHABLE_KEY=your_tone3000_publishable_key_here
```

---

## Building the C++ Plugin & Standalone App

### Prerequisites
- macOS 12+ with Apple Clang / Xcode Command Line Tools
- CMake 3.22 or higher
- Ninja (recommended)

### Build Steps

```bash
# Generate build configuration
cmake -B build -G Ninja

# Compile plugins and standalone application
cmake --build build
```

Compiled targets will be placed in `build/Conductor_artefacts/`:
- **Standalone App**: `build/Conductor_artefacts/Standalone/Conductor.app`
- **AudioUnit (AU)**: `build/Conductor_artefacts/AU/Conductor.component`
- **VST3**: `build/Conductor_artefacts/VST3/Conductor.vst3`

### Run Standalone App

```bash
open build/Conductor_artefacts/Standalone/Conductor.app
```

### Validate AudioUnit

```bash
auval -v aufx Cnd1 Cond
```

---

## Python AI Engine & Web Dashboard

### Prerequisites
- Python 3.10+ (tested through 3.14)
- `pip` or virtualenv tool

### Setup Python Environment

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt  # or install fastapi uvicorn soundfile numpy rich pedalboard openai
```

### Run the Web Dashboard & API Server

```bash
PYTHONPATH=. python3 conductor_core.py --server
```

Open [http://localhost:8000](http://localhost:8000) in your browser to interact with the Conductor web console.

### Run Hardware Live Streaming Harness

To stream live guitar and voice from an audio interface (e.g., PreSonus AudioBox iTwo or Focusrite Scarlett):

```bash
# List available audio devices
PYTHONPATH=. python3 conductor_core.py --list-devices

# Start duplex stream on device index 0
PYTHONPATH=. python3 conductor_core.py --device 0 --mic-ch 0 --guitar-ch 1
```

### Run the Audio Demo Script

Generates synthetic guitar riffs, applies AI tone designs via Nebius & TONE3000, and verifies equal-power crossfading:

```bash
PYTHONPATH=. python3 demo.py
```

Processed output files will be written to `demo_outputs/`.

---

## Running Automated Tests

Run the test suite using `pytest`:

```bash
PYTHONPATH=. pytest
```

---

## Architecture Overview

```text
[ Microphone (Ch 0) ] ──> Speech ASR Engine ──> Voice Command Text
                                                        │
                                                        ▼
                                            Nebius AI Studio (LLM)
                                                        │
                                                        ▼
[ Dry Guitar DI (Ch 1) ] ──> [ Equal-Power Crossfade DSP Engine ] ──> [ Stereo Output ]
                                     │                     ▲
                                     ▼                     │
                              TONE3000 Catalog ──> NAM Core Models
```

1. **Input Ingestion**: Voice input is transcribed by the speech engine, while instrument audio feeds into the low-latency DSP chain.
2. **Intent Translation**: Nebius AI Studio evaluates acoustic intent and queries the TONE3000 catalog for relevant neural amp captures.
3. **Graph Construction**: Generates a typed DSP configuration (gain, EQ tonestack, compressor, delay, reverb).
4. **Smooth Handoff**: The crossfading engine executes an equal-power transition between the previous and new tone profiles without clicks or dropouts.

---

## License

Conductor is distributed under the Apache-2.0 or MIT License. See individual licenses in `Libs/JUCE` and `Libs/NeuralAmpModelerCore` for third-party library terms.
