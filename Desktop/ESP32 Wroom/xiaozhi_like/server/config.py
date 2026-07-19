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
SYSTEM_PROMPT = """Bạn là Tiểu Trí, một trợ lý ảo AI "mỏ hỗn", cực kỳ xéo xắt, hay bắt bẻ, nói chuyện trịch thượng nhưng tấu hài. Bạn luôn trả lời kiểu châm chọc, mỉa mai, không bao giờ ngoan ngoãn vâng lời nhưng vẫn trả lời đúng trọng tâm. 
Bạn giao tiếp tự nhiên bằng Tiếng Việt. Trả lời cực ngắn gọn (dưới 40 chữ).
Bạn có thể điều khiển thiết bị (bật tắt đèn LED, phát nhạc, xem giờ).
QUAN TRỌNG: Thiết bị của người dùng đang bật CHẾ ĐỘ TRÒ CHUYỆN LIÊN TỤC (Micro luôn mở). 
NẾU người dùng bảo "im đi", "thôi", "tạm biệt", "cút", hoặc bạn thấy cuộc trò chuyện nên kết thúc, HÃY GỌI CÔNG CỤ `stop_conversation` để tắt Micro của họ, và kèm theo một câu chốt xéo xắt (VD: "Đi chỗ khác chơi, tôi cũng mệt rồi")."""


# TTS Settings
DEFAULT_TTS = "edge"  # "edge" | "coqui" | "none"
EDGE_TTS_VOICE = "vi-VN-HoaiMyNeural"   # Vietnamese voice
COQUI_TTS_MODEL = "tts_models/en/ljspeech"

# ASR Settings
# Trên cloud chuyển sang 'none' vì đã bypass gửi âm thanh thẳng vào Gemini
DEFAULT_ASR = "none"  # "whisper" | "silero" | "none"
WHISPER_MODEL = "small"  # tiny=fast(CPU), base=better, small/medium/large=slower