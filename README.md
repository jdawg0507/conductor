# Conductor 🎸⚡️

> **Autonomous AI Guitar Sound Designer & Real-Time Neural Amp Processor**

Conductor is an intelligent, real-time guitar audio plugin and standalone macOS workstation that translates natural language tone descriptions into authentic neural amp models, studio acoustics, and dynamic analog pedalboard chains. 

Powered by **JUCE 8**, **C++20**, **Neural Amp Modeler Core (NAM)**, and **Nebius AI Studio**, Conductor bridges natural language conversation and low-latency audio processing—letting guitarists type requests like *"80s stadium metal with tight delay and mid-boost"* or *"glassy Mayer clean with warm room reverb"* and instantly hear an authentic, fully staged rig.

---

## Key Features

- **Neural Amp Modeling (NAM)**: Powered by `NeuralAmpModelerCore` for state-of-the-art machine-learning amplifier and speaker cabinet emulation with near-zero latency.
- **Conversational Tone Architect**: Conversational sound engineering via Nebius AI Studio (`DeepSeek-V4.1-Flash` or OpenAI-compatible endpoints) that analyzes musical genre, artist tone recipes, and staging semantics.
- **TONE3000 Ecosystem Integration**: Embedded catalog mapping of community and studio NAM captures across iconic amplifiers, overdrives, fuzzes, modulations, and reverbs with automatic background model caching.
- **4-Slot Hardware Rack**:
  - **Slot 0 & Slot 1 (Stompboxes)**: Pre-amp pedals (Overdrive, Fuzz, Distortion, Chorus, Phaser, Compressor, Gate).
  - **Slot 2 (Amplifier)**: High-headroom cleans to crushing high-gain tube heads with dynamic tone stacks (Gain, Bass, Mid, Treble, Output).
  - **Slot 3 (Space / Cabinet)**: Studio acoustic rooms, impulse responses (IRs), and vintage tape echoes.
- **Intelligent Audio Telemetry & Routing**:
  - **Smart Input Detection**: Automatically detects active instrument signal between audio interface input channels (Auto, Ch 1, Ch 2, or Sum).
  - **Chromatic Pitch Tuner**: Ultra-responsive pitch detection using time-domain autocorrelation with cents deviation and note lock indicator.
  - **Calibrated Level Metering**: Multi-stage peak and RMS metering with dBFS readouts and clip detection.
- **Live Performance UI & HUD**:
  - **Siri-Style AI Dialog Overlay**: Pulsing holographic status orb, matched semantic tags, and architectural reasoning display.
  - **Reactive Signal Flow**: High-visibility audio patch cables behind gear cards that illuminate dynamically with your playing intensity.
  - **New Tone vs. Edit Tone Modes**: Synthesize an entirely new rig from scratch or iteratively tweak the active tone (e.g. *"add more gain and brighten the top end"*).
  - **Live Debug Console**: Collapsible real-time diagnostic console streaming internal DSP and API events.

---

## Architecture Overview

```text
                      ┌──────────────────────────────────────────────┐
                      │             User Prompt / Voice              │
                      │  "80s high-gain lead with tight mid-boost"   │
                      └──────────────────────┬───────────────────────┘
                                             │
                                             ▼
                      ┌──────────────────────────────────────────────┐
                      │         Nebius AI Studio (LLM Client)        │
                      │    DeepSeek-V4.1-Flash / OpenAI Protocol     │
                      └──────────────────────┬───────────────────────┘
                                             │
                       Structured JSON Rig Recipe & Sonic Rationale
                                             │
                                             ▼
                      ┌──────────────────────────────────────────────┐
                      │              TONE3000 Catalog                │
                      │      ModelCache & Background Fetcher         │
                      └──────────────────────┬───────────────────────┘
                                             │
                                             ▼
┌──────────────────┐   ┌────────────────────────────────────────────────────────┐   ┌───────────────────┐
│ Guitar Input     │──▶│              Conductor NAM Series Chain                │──▶│ Stereo Output     │
│ (Auto Ch Detect) │   │  [Slot 0: Pedal] ➔ [Slot 1: Pedal] ➔ [Slot 2: Amp] ... │   │ (Main Out / Cans) │
└────────┬─────────┘   └────────────────────────────┬───────────────────────────┘   └───────────────────┘
         │                                          │
         ▼                                          ▼
┌──────────────────┐                       ┌────────────────────────────────────┐
│ Autocorrelation  │                       │         Telemetry Engine           │
│ Chromatic Tuner  │                       │   Peak / RMS Meters & Dynamic Glow │
└──────────────────┘                       └────────────────────────────────────┘
```

---

## Repository Structure

```text
Conductor/
├── Source/
│   ├── AI/
│   │   ├── LLMClient.h             # Async Nebius AI Studio client & prompt handler
│   │   ├── ModelCache.h            # Local disk cache & background NAM model fetcher
│   │   └── Tone3000Catalog.h       # Curated TONE3000 gear catalog & tag resolution
│   ├── Audio/
│   │   ├── NamChain.h              # 4-slot NAM processing engine & EQ tone stack
│   │   ├── Telemetry.h             # Lock-free atomic metering and pitch telemetry
│   │   └── TunerDetector.h         # Autocorrelation chromatic pitch detector
│   ├── UI/
│   │   ├── AIDialogOverlay.h       # Siri-style holographic synthesis overlay
│   │   ├── ConductorLookAndFeel.h  # Aerospace titanium styling & rotary knob renderer
│   │   ├── DebugConsoleComponent.h # Embedded live diagnostic terminal
│   │   ├── DebugLog.h              # Thread-safe ring buffer log broadcaster
│   │   ├── LevelMeterComponent.h   # Multi-segment peak / RMS audio meter
│   │   ├── PedalCardComponent.h    # Gear rack slot cards, dynamic knobs & bypass
│   │   ├── StandaloneAudioHelper.h # Standalone audio device & input unmuting helper
│   │   └── TunerComponent.h        # Circular tuner HUD with strobe-style needle
│   ├── PluginProcessor.cpp         # Main JUCE AudioProcessor & lifecycle hooks
│   ├── PluginProcessor.h           # Audio processor definition
│   ├── PluginEditor.cpp            # Main JUCE AudioProcessorEditor & UI layout
│   └── PluginEditor.h              # Editor definition
├── Libs/                           # Git Submodules
│   ├── JUCE/                       # JUCE 8 framework
│   └── NeuralAmpModelerCore/       # Neural Amp Modeler C++ engine
├── CMakeLists.txt                  # Modern CMake build configuration (C++20)
└── .env.example                    # Template for API keys and endpoint settings
```

---

## Getting Started

### 1. Clone the Repository with Submodules

Conductor relies on `JUCE` and `NeuralAmpModelerCore` as git submodules:

```bash
git clone --recurse-submodules https://github.com/jdawg0507/conductor.git
cd conductor
```

If you cloned without `--recurse-submodules`, initialize them with:

```bash
git submodule update --init --recursive
```

### 2. Configure Environment Credentials

Create a `.env` file in the project root based on `.env.example`:

```bash
cp .env.example .env
```

Add your Nebius AI Studio and TONE3000 keys:

```ini
NEBIUS_API_KEY=your_nebius_api_key_here
NEBIUS_BASE_URL=https://api.studio.nebius.ai/v1
NEBIUS_LLM_MODEL=deepseek-ai/DeepSeek-V4.1-Flash
TONE3000_API_KEY=your_tone3000_api_key_here
TONE3000_PUBLISHABLE_KEY=your_tone3000_publishable_key_here
```

*(Note: If no API key is provided, Conductor gracefully falls back to local heuristic matching using its built-in offline capture catalog).*

---

## Building from Source

### Prerequisites
- **macOS**: macOS 12 (Monterey) or higher
- **Compiler**: Apple Clang with C++20 support (Xcode Command Line Tools)
- **Build Tools**: CMake 3.22+ and Ninja

```bash
brew install cmake ninja
```

### Build Commands

```bash
# Configure the build directory with Ninja
cmake -B build -G Ninja

# Compile all targets (Standalone, AU, VST3)
cmake --build build
```

Compiled deliverables are located in `build/Conductor_artefacts/`:
- **macOS Standalone Application**: `build/Conductor_artefacts/Standalone/Conductor.app`
- **AudioUnit (AU)**: `build/Conductor_artefacts/AU/Conductor.component`
- **VST3 Plugin**: `build/Conductor_artefacts/VST3/Conductor.vst3`

---

## Running & Validating

### Launch the Standalone Application

```bash
open build/Conductor_artefacts/Standalone/Conductor.app
```

Click **AUDIO SETUP** in the top navigation bar to select your audio interface (e.g., Focusrite Scarlett, Universal Audio Apollo, PreSonus AudioBox), sample rate, and buffer size.

### Validate the AudioUnit (AU)

Validate the plugin using Apple's `auval` utility:

```bash
auval -v aufx Cnd1 Cond
```

---

## Usage Guide

1. **Describe a Tone**: Type your sonic vision into the top prompt bar:
   - *"Texas blues lead with subtle tube overdrive and warm spring reverb"*
   - *"Tight percussive djent rhythm with noise gate and mid-focused bite"*
   - *"Glassy 80s chorus clean with lush stereo plate"*
2. **Conduct Tone**: Press Enter or click **CONDUCT TONE**. The AI overlay analyzes the request, selects appropriate models from the TONE3000 catalog, stages the 4 rack slots, dials in starting knob positions, and displays its design rationale.
3. **Refine & Tweak**:
   - Turn individual knobs on any active stompbox or amp card.
   - Click **ENGAGE** on any pedal card to bypass or re-activate it.
   - Switch mode to **EDIT TONE** to iteratively adjust the active sound (e.g., *"give it a bit more mid-presence and lower the gain"*).
4. **Input Selection**: Click **IN: AUTO** to cycle between smart signal detection, Channel 1, Channel 2, or stereo summing.

---

## License

Conductor is open source under the MIT License. Third-party dependencies are licensed under their respective terms:
- [JUCE 8](file:///Users/jdawg/Conductor/Libs/JUCE/LICENSE.md) — JUCE License / GPLv3
- [NeuralAmpModelerCore](file:///Users/jdawg/Conductor/Libs/NeuralAmpModelerCore/LICENSE) — Apache-2.0
