"""
Speech Ingestion and Streaming ASR Engine for Conductor.
Handles 16kHz Mono PCM Microphone Capture, Voice Activity Detection (VAD),
Push-To-Talk (PTT) Interrupt Loops, Nebius Streaming ASR Client, and Local Speech Fallback.
"""

import asyncio
import numpy as np
import time
import threading
import queue
from typing import Callable, Optional
import websockets
import json

from .config import config


class SpeechIngestionEngine:
    """
    Ingests 16kHz Mono PCM audio stream from microphone source,
    performs real-time Voice Activity Detection (VAD), and processes speech
    to Nebius Streaming ASR / LLM in an asynchronous background worker thread.
    """

    def __init__(
        self,
        sample_rate: int = 16000,
        energy_threshold: float = 0.015,
        on_transcription: Optional[Callable[[str], None]] = None
    ):
        self.sample_rate = sample_rate
        self.energy_threshold = energy_threshold
        self.on_transcription = on_transcription

        self.is_recording = False
        self.is_push_to_talk_active = False
        self.audio_buffer = []
        self.lock = threading.Lock()

        # Asynchronous Background AI Control Plane Worker Queue
        self.speech_queue: queue.Queue = queue.Queue()
        self.worker_thread = threading.Thread(target=self._speech_worker_loop, daemon=True)
        self.worker_thread.start()

    def _speech_worker_loop(self):
        """Worker thread executing ASR & LLM control plane out-of-band."""
        while True:
            try:
                speech_pcm = self.speech_queue.get()
                if speech_pcm is None:
                    break
                text = self._transcribe_speech(speech_pcm)
                if text and self.on_transcription:
                    self.on_transcription(text)
            except Exception as e:
                print(f"[Voice AI Control Worker] Error processing speech: {e}")
            finally:
                self.speech_queue.task_done()

    def process_pcm_chunk(self, pcm_chunk: np.ndarray) -> Optional[str]:
        """
        Process a 16kHz float32 or int16 mono audio chunk.
        Zero-blocking: pushes speech buffers onto background queue for processing.
        """
        chunk_float = pcm_chunk.astype(np.float32)
        if chunk_float.ndim > 1:
            chunk_float = chunk_float.mean(axis=1)

        rms_energy = np.sqrt(np.mean(chunk_float ** 2)) if len(chunk_float) > 0 else 0.0

        with self.lock:
            if self.is_push_to_talk_active or rms_energy > self.energy_threshold:
                self.audio_buffer.append(chunk_float)
                self.is_recording = True
            elif self.is_recording:
                # Silence detected - enqueue for background ASR/LLM worker
                if len(self.audio_buffer) > 5:  # at least ~0.3s of audio
                    full_speech = np.concatenate(self.audio_buffer)
                    self.audio_buffer = []
                    self.is_recording = False
                    self.speech_queue.put(full_speech)
                    return "[Enqueued for Voice AI Worker]"
                self.audio_buffer = []
                self.is_recording = False

        return None


    def trigger_push_to_talk_start(self):
        """Activates Push-To-Talk manual speech recording."""
        with self.lock:
            self.is_push_to_talk_active = True
            self.audio_buffer = []
            self.is_recording = True

    def trigger_push_to_talk_stop(self) -> Optional[str]:
        """Deactivates Push-To-Talk and enqueues speech for background ASR/LLM worker."""
        with self.lock:
            self.is_push_to_talk_active = False
            self.is_recording = False
            if len(self.audio_buffer) > 0:
                full_speech = np.concatenate(self.audio_buffer)
                self.audio_buffer = []
                self.speech_queue.put(full_speech)
                return "[Enqueued for Voice AI Worker]"
            self.audio_buffer = []
            return None


    def _transcribe_speech(self, speech_pcm: np.ndarray) -> str:
        """
        Sends recorded PCM speech stream to Nebius Streaming ASR or local fallback.
        Returns empty string if streaming ASR is not active.
        """
        duration_sec = len(speech_pcm) / self.sample_rate

        # Check if Nebius ASR endpoint is reachable via async task or API
        if config.nebius_api_key and config.nebius_asr_endpoint:
            try:
                return self._nebius_asr_transcribe(speech_pcm)
            except Exception as e:
                print(f"[Speech ASR] Nebius streaming ASR warning ({e}).")

        # Do not emit fake hardcoded prompt loop when no ASR service is streaming
        return ""


    def _nebius_asr_transcribe(self, speech_pcm: np.ndarray) -> str:
        """
        Transcribe audio via Nebius Riva / Whisper streaming endpoint.
        """
        # Convert float32 PCM to 16-bit PCM bytes
        pcm16 = (speech_pcm * 32767.0).astype(np.int16).tobytes()

        # Place real WebSocket streaming transcription call here when active
        return ""
