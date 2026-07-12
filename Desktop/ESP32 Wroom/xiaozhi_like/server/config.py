# P8 Server Configuration
# Edit this file with your API keys

import os

# API Keys
# Đọc key từ biến môi trường để bảo mật trên Cloud
OPENAI_API_KEY = os.getenv("OPENAI_API_KEY", "")
OPENAI_BASE_URL = "https://generativelanguage.googleapis.com/v1beta/openai/"
OLLAMA_URL = os.getenv("OLLAMA_URL", "http://localhost:11434")

# Server Settings
HOST = "0.0.0.0"
PORT = 8080

# Audio Settings
SAMPLE_RATE = 16000  # Match ESP32 mic sample rate
AUDIO_CHANNELS = 1

# LLM Settings
DEFAULT_LLM = "openai"  # "ollama" | "openai"
OLLAMA_MODEL = os.getenv("OLLAMA_MODEL", "llama3.2")
OPENAI_MODEL = "gemini-1.5-flash"
SYSTEM_PROMPT = "Bạn là Tiểu Trí, một trợ lý ảo thông minh, luôn trả lời ngắn gọn và súc tích bằng tiếng Việt."

# TTS Settings
DEFAULT_TTS = "edge"  # "edge" | "coqui" | "none"
EDGE_TTS_VOICE = "vi-VN-HoaiMyNeural"   # Vietnamese voice
COQUI_TTS_MODEL = "tts_models/en/ljspeech"

# ASR Settings
# Trên cloud chuyển sang 'none' vì đã bypass gửi âm thanh thẳng vào Gemini
DEFAULT_ASR = "none"  # "whisper" | "silero" | "none"
WHISPER_MODEL = "small"  # tiny=fast(CPU), base=better, small/medium/large=slower