#!/usr/bin/env python3
"""
Conductor Execution Harness (conductor_core.py)
Intelligent TONE3000 Plugin Wrapper & Native Hardware Streamer

Configured for 2-In 1-Out Audio Interface (e.g. PreSonus AudioBox iTwo):
  - Input 1 (Ch 0): Voice Control Mic (Speech ASR Engine)
  - Input 2 (Ch 1): Guitar Instrument (TONE3000 NAM + Pedalboard DSP)
  - Output 1 & 2: Main Outputs / Headphones (Bit-perfect Native Passthrough)
"""

import sys
import os
import time
import argparse
import asyncio
import numpy as np
from rich.console import Console
from rich.panel import Panel
from rich.table import Table

from conductor.config import config
from conductor.schemas import VoiceCommandRequest, DSPGraphConfig
from conductor.nebius_client import NebiusSoundEngineerClient
from conductor.dsp_engine import CrossfadeDSPEngine
from conductor.asr_engine import SpeechIngestionEngine
from conductor.audio_io import get_available_devices, Tone3000HardwareDuplexStream, process_audio_file
from conductor.server import start_server

console = Console()


class ConductorCoreEngine:
    """
    Main execution harness coordinating TONE3000 captures, Nebius AI voice agent,
    native hardware duplex stream, and dynamic Pedalboard DSP engine.
    """

    def __init__(self, device_id=None, mic_ch=0, guitar_ch=1):
        self.dsp_engine = CrossfadeDSPEngine(sample_rate=config.sample_rate)
        self.nebius_client = NebiusSoundEngineerClient()
        self.asr_engine = SpeechIngestionEngine(
            sample_rate=config.asr_sample_rate,
            on_transcription=self.on_voice_transcription
        )
        self.hardware_stream = Tone3000HardwareDuplexStream(
            dsp_engine=self.dsp_engine,
            asr_engine=self.asr_engine,
            device_id=device_id,
            mic_ch=mic_ch,
            guitar_ch=guitar_ch
        )

    def start_audio_engine(self):
        """Starts native audio interface hardware stream."""
        self.hardware_stream.start()

    def stop_audio_engine(self):
        """Stops hardware stream."""
        self.hardware_stream.stop()

    def process_aesthetic_command(self, command: str) -> DSPGraphConfig:
        """
        Process prompt, queries Nebius AI / TONE3000 API,
        synthesizes/tunes dynamic graph, and mutates DSP engine cleanly.
        """
        console.print(f"\n[bold cyan]🎙 Live Voice Prompt:[/bold cyan] '{command}'")
        t0 = time.perf_counter()

        res = self.nebius_client.process_command(
            command=command,
            current_graph=self.dsp_engine.current_config
        )
        graph = res.graph

        # Mutate DSP Engine with equal-power crossfader
        t_cross0 = time.perf_counter()
        self.dsp_engine.update_graph(graph)
        crossfade_ms = (time.perf_counter() - t_cross0) * 1000.0

        total_ms = (time.perf_counter() - t0) * 1000.0

        self._display_summary_table(command, graph, total_ms, res.latency_ms, crossfade_ms, res.tuning_action, res.changes_summary)
        return graph

    def on_voice_transcription(self, text: str):
        """Callback invoked when streaming voice mic ASR completes a phrase."""
        if not text or not text.strip():
            return
        clean_text = text.strip()
        if hasattr(self, '_last_voice_cmd') and self._last_voice_cmd == clean_text and (time.time() - getattr(self, '_last_voice_time', 0)) < 4.0:
            return
        self._last_voice_cmd = clean_text
        self._last_voice_time = time.time()
        console.print(f"\n[bold green]Voice Mic Transcribed:[/bold green] '{clean_text}'")
        self.process_aesthetic_command(clean_text)

    def _display_summary_table(
        self,
        cmd: str,
        graph: DSPGraphConfig,
        total_ms: float,
        ai_ms: float,
        crossfade_ms: float,
        tuning_action: str = "board_created",
        changes_summary: str = ""
    ):
        title_action = "Live Tuned" if tuning_action == "live_tuned" else "Board Created"
        table = Table(title=f"TONE3000 AI [{title_action}]: '{cmd}'", header_style="bold yellow")
        table.add_column("Stage / Element", style="cyan")
        table.add_column("Details / Topology", style="bold green")
        table.add_column("Latency / Status", style="dim")

        table.add_row("Nebius AI Reasoning", self.nebius_client.model if self.nebius_client.openai_client else "Local Semantic Engine", f"{ai_ms:.2f} ms")
        table.add_row("Equal-Power Crossfade", "Double-buffered 512-sample swap", f"{crossfade_ms:.2f} ms")
        table.add_row("End-to-End Latency", f"Target <{config.target_max_latency_ms}ms", f"[bold green]{total_ms:.2f} ms[/bold green]")
        table.add_row("Master Output Stage", "Master Gain", f"{graph.global_gain_db:+.1f} dB")
        table.add_row("AI Intent Rationale", graph.intent, "Verified")

        if getattr(graph, "gear_selection_rationale", None):
            table.add_row("Gear Curation", graph.gear_selection_rationale, "Curated")

        if changes_summary:
            table.add_row("Tuning Summary", changes_summary, title_action)

        node_str = " -> ".join([f"{n.display_name} [{n.gear_model or n.node_type}]" if n.gear_model else (n.display_name or n.node_type) for n in graph.nodes if n.enabled])
        table.add_row("Synthesized Chain", node_str, f"{len(graph.nodes)} Dynamic Nodes")

        if graph.emotional_tags:
            table.add_row("Emotional Metaphors", ", ".join(graph.emotional_tags), "Tags Extracted")

        console.print(table)


def display_devices_table():
    """Prints formatted table of connected audio hardware devices."""
    dev_data = get_available_devices()
    devices = dev_data.get("devices", []) if isinstance(dev_data, dict) else dev_data

    t_dev = Table(title="Connected Audio Hardware Interfaces", header_style="bold magenta")
    t_dev.add_column("ID", style="bold yellow")
    t_dev.add_column("Device Name")
    t_dev.add_column("Inputs", style="cyan")
    t_dev.add_column("Outputs", style="green")
    t_dev.add_column("Sample Rate")

    for d in devices:
        t_dev.add_row(
            str(d["id"]),
            d["name"],
            str(d.get("max_input_channels", 0)),
            str(d.get("max_output_channels", 0)),
            f"{d.get('default_samplerate', 48000)} Hz"
        )

    console.print(t_dev)


def main():
    parser = argparse.ArgumentParser(description="Conductor TONE3000 Plugin Wrapper")
    parser.add_argument("--command", "-c", type=str, help="Abstract, emotional, or technical prompt to process")
    parser.add_argument("--list-devices", "-l", action="store_true", help="Display all connected hardware audio devices")
    parser.add_argument("--device", "-d", type=str, help="Hardware interface device ID or name")
    parser.add_argument("--mic-channel", type=int, default=0, help="Input 1 Voice Mic channel index (0-indexed, default: 0)")
    parser.add_argument("--guitar-channel", type=int, default=1, help="Input 2 Guitar channel index (0-indexed, default: 1)")
    parser.add_argument("--audio-file", type=str, help="Input WAV/MP3 guitar audio file to process through DSP engine")
    parser.add_argument("--server", "-s", action="store_true", help="Start Web Dashboard & REST API server")
    parser.add_argument("--port", "-p", type=int, default=7860, help="Web server port")

    args = parser.parse_args()

    if args.list_devices:
        display_devices_table()
        sys.exit(0)

    dev_id = int(args.device) if args.device and args.device.isdigit() else args.device

    engine = ConductorCoreEngine(
        device_id=dev_id,
        mic_ch=args.mic_channel,
        guitar_ch=args.guitar_channel
    )

    if args.audio_file:
        console.print(f"\n[bold yellow]Processing Audio File:[/bold yellow] {args.audio_file}")
        if args.command:
            engine.process_aesthetic_command(args.command)
        with open(args.audio_file, "rb") as f:
            in_bytes = f.read()
        out_bytes, duration = process_audio_file(in_bytes, engine.dsp_engine)
        out_filename = f"processed_{os.path.basename(args.audio_file)}"
        with open(out_filename, "wb") as f:
            f.write(out_bytes)
        console.print(f"[bold green]Saved processed audio file ({duration:.2f}s) to:[/bold green] {out_filename}")
        sys.exit(0)

    engine.start_audio_engine()

    try:
        if args.command:
            engine.process_aesthetic_command(args.command)

        elif args.server:
            console.print(f"[bold cyan]Starting Conductor Web Studio on http://127.0.0.1:{args.port}...[/bold cyan]")
            start_server(host="127.0.0.1", port=args.port, core_engine_instance=engine)

        else:
            console.print(Panel(
                "[bold cyan]CONDUCTOR: INTELLIGENT TONE3000 PLUGIN WRAPPER[/bold cyan]\n"
                "Hardware Interface Routing (2-In 1-Out):\n"
                f"  - Input 1 (Ch {args.mic_channel + 1}): Voice Control Mic -> Speech ASR Engine\n"
                f"  - Input 2 (Ch {args.guitar_channel + 1}): Guitar Line-In -> TONE3000 / Pedalboard DSP\n"
                f"  - Output 1 & 2: Monitor Speakers / Headphones\n\n"
                "Type a prompt (e.g. 'dreamy shoegaze wall of sound' or 'more gain and cut the treble'),\n"
                "or type 'devices' to list hardware, 'web' to launch Web Studio, 'exit' to quit.",
                title="Conductor TONE3000 Console"
            ))

            while True:
                try:
                    user_input = input("\nConductor AI > ").strip()
                    if not user_input:
                        continue
                    if user_input.lower() in ["exit", "quit"]:
                        break
                    elif user_input.lower() in ["devices", "list"]:
                        display_devices_table()
                    elif user_input.lower() == "web":
                        start_server(host="127.0.0.1", port=args.port, core_engine_instance=engine)
                        break
                    else:
                        engine.process_aesthetic_command(user_input)
                except (KeyboardInterrupt, EOFError):
                    break

    finally:
        engine.stop_audio_engine()
        console.print("[bold yellow]TONE3000 hardware engine shut down gracefully.[/bold yellow]")


if __name__ == "__main__":
    main()
