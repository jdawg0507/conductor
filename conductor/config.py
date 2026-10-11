import os
from typing import Optional, Union
from pydantic import BaseModel, Field

def _load_env_file():

    env_path = os.path.join(os.path.dirname(os.path.dirname(__file__)), ".env")
    if os.path.exists(env_path):
        try:
            with open(env_path, "r") as f:
                for line in f:
                    line = line.strip()
                    if line and not line.startswith("#") and "=" in line:
                        k, v = line.split("=", 1)
                        os.environ[k.strip()] = v.strip()
        except Exception:
            pass

_load_env_file()


class ConductorConfig(BaseModel):
    # Nebius Token Factory Settings
    nebius_api_key: str = Field(
        default_factory=lambda: os.getenv("NEBIUS_API_KEY", "")
    )
    nebius_base_url: str = Field(
        default_factory=lambda: os.getenv("NEBIUS_BASE_URL", "https://api.studio.nebius.ai/v1")
    )
    nebius_llm_model: str = Field(
        default_factory=lambda: os.getenv("NEBIUS_LLM_MODEL", "deepseek-ai/DeepSeek-V4.1-Flash")
    )
    nebius_asr_endpoint: str = Field(
        default_factory=lambda: os.getenv("NEBIUS_ASR_ENDPOINT", "wss://api.studio.nebius.ai/v1/asr/streaming")
    )

    # TONE3000 Platform API Settings
    tone3000_api_key: str = Field(
        default_factory=lambda: os.getenv("TONE3000_API_KEY", "")
    )
    tone3000_publishable_key: str = Field(
        default_factory=lambda: os.getenv("TONE3000_PUBLISHABLE_KEY", "")
    )
    tone3000_base_url: str = Field(
        default_factory=lambda: os.getenv("TONE3000_BASE_URL", "https://www.tone3000.com/api/v1")
    )

    # 2 Inputs + 1 Output Routing Settings
    mic_device_id: Optional[Union[int, str]] = None      # Input 1: Voice Control Mic
    guitar_device_id: Optional[Union[int, str]] = None   # Input 2: Guitar Line-In
    speaker_device_id: Optional[Union[int, str]] = None  # Output 1: Speakers / Headphones

    guitar_channel_idx: int = 0  # Channel pin for guitar
    mic_channel_idx: int = 0     # Channel pin for mic

    # Audio Settings
    sample_rate: int = 48000
    asr_sample_rate: int = 16000
    channels: int = 2
    buffer_size: int = 512
    crossfade_samples: int = 512  # Equal-power crossfade window (~10.6ms @ 48kHz)
    slew_time_ms: float = 20.0

    # Latency Budget Constraints (in milliseconds)
    target_max_latency_ms: float = 400.0


    # Web Server Settings
    web_host: str = "127.0.0.1"
    web_port: int = 7860


config = ConductorConfig()
