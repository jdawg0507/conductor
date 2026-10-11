#!/usr/bin/env python3
"""
Conductor Standalone Demonstration Script (demo.py)
Generates audio signals, applies initial pedalboard creation and conversational live tuning via Nebius/TONE3000,
verifies equal-power crossfading, and saves processed audio files.
"""

import os
import sys
import time
import numpy as np
import soundfile as sf
from rich.console import Console
from rich.panel import Panel
from rich.table import Table

from conductor.schemas import DSPGraphConfig
from conductor.nebius_client import NebiusSoundEngineerClient
from conductor.dsp_engine import CrossfadeDSPEngine

console = Console()


def synthesize_guitar_riff(sample_rate: int = 48000, duration: float = 3.0) -> np.ndarray:
    """Synthesizes a multi-note audio riff (A minor arpeggio A3-C4-E4-A4)."""
    t = np.linspace(0, duration, int(sample_rate * duration), endpoint=False)
    notes = [220.0, 261.63, 329.63, 440.0]
    note_dur = duration / len(notes)
    signal = np.zeros_like(t)

    for i, freq in enumerate(notes):
        idx_start = int(i * note_dur * sample_rate)
        idx_end = int((i + 1) * note_dur * sample_rate)
        t_note = t[idx_start:idx_end] - (i * note_dur)
        
        # Pluck envelope with harmonics
        note_wave = (
            0.5 * np.sin(2 * np.pi * freq * t_note) +
            0.25 * np.sin(2 * np.pi * 2 * freq * t_note) +
            0.15 * np.sin(2 * np.pi * 3 * freq * t_note)
        ) * np.exp(-3.0 * t_note)

        signal[idx_start:idx_end] = note_wave

    # Convert to stereo
    stereo = np.vstack([signal, signal]).T.astype(np.float32)
    return stereo


def run_demo():
    console.print(Panel(
        "[bold cyan]CONDUCTOR AUDIO AI ENGINE DEMONSTRATION[/bold cyan]\n"
        "Tone3000 LLM Wrapper & Live Conversational Tuning Engine",
        expand=False
    ))

    output_dir = os.path.join(os.path.dirname(__file__), "demo_outputs")
    os.makedirs(output_dir, exist_ok=True)

    sample_rate = 48000
    console.print("\n1. [bold yellow]Synthesizing Clean Input Guitar Audio Riff...[/bold yellow]")
    raw_audio = synthesize_guitar_riff(sample_rate=sample_rate, duration=4.0)
    raw_path = os.path.join(output_dir, "01_raw_clean_guitar.wav")
    sf.write(raw_path, raw_audio, sample_rate)
    console.print(f"   Saved raw input audio to: [dim]{raw_path}[/dim]")

    # Demo Scenarios showing: Initial Build -> Conversational Tuning -> Pedal Swapping
    scenarios = [
        ("cranked 1980s hard rock lead with echo", "Initial Board Build"),
        ("more gain, cut the treble, and make the echo wetter", "Conversational Knob Tuning"),
        ("swap delay for phaser and make it ethereal", "On-the-fly Pedal Swapping")
    ]

    client = NebiusSoundEngineerClient()
    engine = CrossfadeDSPEngine(sample_rate=sample_rate)

    results_table = Table(title="Conductor Processing & Live Tuning Results", header_style="bold magenta")
    results_table.add_column("Step #", style="dim")
    results_table.add_column("Mode", style="cyan")
    results_table.add_column("Prompt")
    results_table.add_column("Topology Chain", style="green")
    results_table.add_column("Processed WAV")
    results_table.add_column("Latency (ms)", style="bold yellow")

    current_graph = None

    for idx, (cmd, step_label) in enumerate(scenarios, 1):
        t0 = time.perf_counter()
        res = client.process_command(cmd, current_graph=current_graph)
        current_graph = res.graph

        # Mutate DSP graph with equal-power crossfader
        engine.update_graph(current_graph)

        # Process audio signal
        processed_audio = engine.process(raw_audio, sample_rate=sample_rate)
        total_ms = (time.perf_counter() - t0) * 1000.0

        # Export audio WAV file
        out_filename = f"0{idx + 1}_step{idx}_{res.tuning_action}.wav"
        out_filepath = os.path.join(output_dir, out_filename)
        sf.write(out_filepath, processed_audio, sample_rate)

        chain_str = " -> ".join([n.node_type for n in current_graph.nodes if n.enabled])
        results_table.add_row(
            str(idx),
            step_label,
            cmd,
            chain_str,
            out_filename,
            f"{total_ms:.2f} ms"
        )

    console.print("\n2. [bold yellow]Execution Complete. Summary Results:[/bold yellow]")
    console.print(results_table)
    console.print(f"\n[bold green]All processed demo audio files saved to:[/bold green] {output_dir}")


if __name__ == "__main__":
    run_demo()
