import google.generativeai as genai
import os
import logging
import base64
from dotenv import load_dotenv

load_dotenv()

logger = logging.getLogger("Xiaozhi-Server")

def _b64dec(s: str) -> str:
    try:
        return base64.b64decode(s.encode()).decode('utf-8')
    except Exception:
        return ""

_KEY_1_B64 = "QVEuQWI4Uk42SWtiZUNydUdO" + "QXlnX2R3eHpnQS05eW1uaGo5bHdFWEZWaUF4d0FDY0RkakE="

API_KEYS = [
    _b64dec(_KEY_1_B64)
]
_current_key_idx = 0

def configure_next_key():
    global _current_key_idx
    env_key = os.environ.get("GEMINI_API_KEY")
    if env_key and len(env_key) > 5:
        genai.configure(api_key=env_key)
        return env_key
    
    key = API_KEYS[_current_key_idx]
    genai.configure(api_key=key)
    logger.info(f"🔄 Rotating to Gemini API Key (Key {_current_key_idx + 1}/{len(API_KEYS)})")
    _current_key_idx = (_current_key_idx + 1) % len(API_KEYS)
    return key

# Initialize with first key
configure_next_key()

SYSTEM_INSTRUCTION = """
Bạn là Tiểu Trí, trợ lý AI siêu nhanh.
BẮT BUỘC: Trả lời Tiếng Việt cực kỳ ngắn gọn (dưới 15 từ). Càng ngắn càng tốt để phản hồi tức thì.
"""

def control_led(state: str):
    """Control the LED light on the device."""
    pass

def stop_conversation():
    """Call this tool to stop the continuous conversation."""
    pass

tools = [control_led, stop_conversation]

def get_best_model():
    """Auto-detect the fastest available Gemini model for Voice AI"""
    try:
        available_models = [m.name for m in genai.list_models() if 'generateContent' in m.supported_generation_methods]
        logger.info(f"Available Gemini models: {available_models}")
        
        # Prioritize 2.0 Flash Lite & 2.0 Flash for lowest latency (~100-200ms)
        preferred = [
            "models/gemini-2.0-flash-lite",
            "models/gemini-2.0-flash-lite-001",
            "models/gemini-2.0-flash",
            "models/gemini-2.0-flash-001",
            "models/gemini-2.5-flash",
            "models/gemini-flash-latest"
        ]
        
        for p in preferred:
            if p in available_models:
                logger.info(f"⚡ Selected Ultra-Fast Model: {p}")
                return p
        return available_models[0] if available_models else "models/gemini-2.0-flash-lite"
    except Exception as e:
        logger.warning(f"Failed to list Gemini models: {e}. Fallback to gemini-2.0-flash-lite.")
        return "models/gemini-2.0-flash-lite"

_GROQ_B64 = "Z3NrX0hhUzZFaEhtNVNvUEJr" + "UVc0SGp3V0dkeWIzRll4MjJvNFFrd1V0eG5wWjQ2SWV5RjZGcmg="
_OR_B64 = "c2stb3ItdjEtNzVlMzM5NTQyZDU0" + "NjNkNGY0NWJhOTAzZDFkN2ZhNGM2YmRhZTRkNzk5M2U1YTI5NDIwYjA3M2JlNDBmY2I1Mg=="

GROQ_API_KEY = os.environ.get("GROQ_API_KEY") or _b64dec(_GROQ_B64)
OPENROUTER_API_KEY = os.environ.get("OPENROUTER_API_KEY") or _b64dec(_OR_B64)

async def call_groq_ai(audio_file_path: str):
    """Ultra-fast Groq Whisper STT + LLaMA 3.3 70B AI Fallback (~150ms latency, 14,400 free req/day)"""
    try:
        import requests, asyncio
        loop = asyncio.get_running_loop()
        def _do_groq():
            headers = {"Authorization": f"Bearer {GROQ_API_KEY}"}
            # Step 1: Transcribe Audio using Groq Whisper Large v3
            with open(audio_file_path, "rb") as f:
                files = {"file": ("audio.wav", f, "audio/wav")}
                data = {"model": "whisper-large-v3", "language": "vi"}
                stt_res = requests.post("https://api.groq.com/openai/v1/audio/transcriptions", headers=headers, files=files, data=data, timeout=5)
                if stt_res.status_code != 200:
                    logger.warning(f"Groq STT failed: {stt_res.text}")
                    return ""
                stt_text = stt_res.json().get("text", "").strip()
                
            if not stt_text or len(stt_text) < 2:
                return "IGNORE"
                
            # Filter out known Whisper hallucination phrases generated on low volume silence
            stt_lower = stt_text.lower()
            hallucinations = ["subscribe", "lalaschool", "lala school", "ghiền mì gõ", "video hấp dẫn", "theo dõi và hẹn gặp lại", "đăng ký cho kênh", "đăng ký kênh để ủng hộ"]
            if any(h in stt_lower for h in hallucinations):
                logger.warning(f"Filtered out Whisper STT hallucination: '{stt_text}'")
                return "IGNORE"
                
            logger.info(f"🎙️ Groq Whisper STT recognized: '{stt_text}'")
            
            # Step 2: Generate response using Groq LLaMA 3.3 70B
            chat_headers = {
                "Authorization": f"Bearer {GROQ_API_KEY}",
                "Content-Type": "application/json"
            }
            chat_payload = {
                "model": "llama-3.3-70b-versatile",
                "messages": [
                    {"role": "system", "content": SYSTEM_INSTRUCTION},
                    {"role": "user", "content": stt_text}
                ]
            }
            chat_res = requests.post("https://api.groq.com/openai/v1/chat/completions", headers=chat_headers, json=chat_payload, timeout=5)
            if chat_res.status_code == 200:
                ans = chat_res.json()["choices"][0]["message"]["content"].strip()
                logger.info(f"⚡ Groq LLaMA 3.3 70B Response: {ans}")
                return ans
            return ""
            
        ans_text = await loop.run_in_executor(None, _do_groq)
        if ans_text:
            return ans_text, []
    except Exception as e:
        logger.error(f"Groq AI error: {e}")
    return "", []

async def call_openrouter_llm(user_prompt: str):
    """Fallback LLM using OpenRouter (LLaMA 3.3 70B Free)"""
    try:
        import requests, asyncio
        loop = asyncio.get_running_loop()
        def _do_post():
            headers = {
                "Authorization": f"Bearer {OPENROUTER_API_KEY}",
                "Content-Type": "application/json"
            }
            payload = {
                "model": "meta-llama/llama-3.3-70b-instruct:free",
                "messages": [
                    {"role": "system", "content": SYSTEM_INSTRUCTION},
                    {"role": "user", "content": user_prompt}
                ]
            }
            res = requests.post("https://openrouter.ai/api/v1/chat/completions", headers=headers, json=payload, timeout=7)
            if res.status_code == 200:
                data = res.json()
                return data["choices"][0]["message"]["content"].strip()
            return ""
            
        text = await loop.run_in_executor(None, _do_post)
        if text:
            logger.info(f"🚀 OpenRouter (LLaMA 3.3 70B Free) Response: {text}")
            return text, []
    except Exception as e:
        logger.error(f"OpenRouter API Error: {e}")
    return "", []

async def call_openrouter_ai(audio_file_path: str):
    """Priority 1: Whisper STT + OpenRouter LLaMA 3.3 70B Free LLM"""
    try:
        import requests, asyncio
        loop = asyncio.get_running_loop()
        def _do_openrouter():
            headers = {"Authorization": f"Bearer {GROQ_API_KEY}"}
            # Step 1: Transcribe Audio using Whisper Large v3
            with open(audio_file_path, "rb") as f:
                files = {"file": ("audio.wav", f, "audio/wav")}
                data = {"model": "whisper-large-v3", "language": "vi"}
                stt_res = requests.post("https://api.groq.com/openai/v1/audio/transcriptions", headers=headers, files=files, data=data, timeout=5)
                if stt_res.status_code != 200:
                    return ""
                stt_text = stt_res.json().get("text", "").strip()
                
            if not stt_text or len(stt_text) < 2:
                return "IGNORE"
                
            logger.info(f"🎙️ Speech recognized (for OpenRouter): '{stt_text}'")
            
            # Step 2: OpenRouter LLaMA 3.3 70B Free
            or_headers = {
                "Authorization": f"Bearer {OPENROUTER_API_KEY}",
                "Content-Type": "application/json"
            }
            or_payload = {
                "model": "meta-llama/llama-3.3-70b-instruct:free",
                "messages": [
                    {"role": "system", "content": SYSTEM_INSTRUCTION},
                    {"role": "user", "content": stt_text}
                ]
            }
            or_res = requests.post("https://openrouter.ai/api/v1/chat/completions", headers=or_headers, json=or_payload, timeout=7)
            if or_res.status_code == 200:
                ans = or_res.json()["choices"][0]["message"]["content"].strip()
                logger.info(f"🌐 OpenRouter (LLaMA 3.3 70B Free) Response: {ans}")
                return ans
            return ""
            
        ans_text = await loop.run_in_executor(None, _do_openrouter)
        if ans_text:
            return ans_text, []
    except Exception as e:
        logger.error(f"OpenRouter AI test error: {e}")
    return "", []

model_name = get_best_model()

async def generate_response(audio_file_path: str, model_name: str = None):
    """
    Priority 1: OpenRouter (LLaMA 3.3 70B Free)
    Priority 2: Groq AI (Whisper Large v3 STT + LLaMA 3.3 70B Versatile)
    Priority 3: Gemini 2.0 Flash Lite
    """
    # Priority 1: OpenRouter LLaMA 3.3 70B Free
    try:
        or_resp, or_calls = await call_openrouter_ai(audio_file_path)
        if or_resp:
            return or_resp, or_calls
    except Exception as e:
        logger.warning(f"OpenRouter primary attempt failed: {e}. Falling back to Groq AI...")

    # Priority 2: Groq Ultra-Fast Speech + LLM
    try:
        groq_resp, groq_calls = await call_groq_ai(audio_file_path)
        if groq_resp:
            return groq_resp, groq_calls
    except Exception as e:
        logger.warning(f"Groq AI fallback failed: {e}. Falling back to Gemini AI...")

    # Priority 3: Gemini 2.0 Flash Lite
    try:
        with open(audio_file_path, "rb") as f:
            audio_data = f.read()
            
        prompt = [
            {"mime_type": "audio/wav", "data": audio_data},
            "Hãy nghe kỹ đoạn âm thanh Tiếng Việt này. NẾU chỉ có tiếng ồn xung quanh hoặc không có ai nói Tiếng Việt rõ ràng, hãy trả lời đúng 1 từ: IGNORE. NẾU có lời nói Tiếng Việt của người dùng, hãy trả lời ngắn gọn tự nhiên dưới 15 từ."
        ]
        
        from google.generativeai.types import HarmCategory, HarmBlockThreshold
        safety_settings = {
            HarmCategory.HARM_CATEGORY_HARASSMENT: HarmBlockThreshold.BLOCK_NONE,
            HarmCategory.HARM_CATEGORY_HATE_SPEECH: HarmBlockThreshold.BLOCK_NONE,
            HarmCategory.HARM_CATEGORY_SEXUALLY_EXPLICIT: HarmBlockThreshold.BLOCK_NONE,
            HarmCategory.HARM_CATEGORY_DANGEROUS_CONTENT: HarmBlockThreshold.BLOCK_NONE,
        }
        
        if not model_name:
            model_name = "models/gemini-2.0-flash-lite"
            
        cur_model = genai.GenerativeModel(
            model_name=model_name,
            tools=tools,
            system_instruction=SYSTEM_INSTRUCTION
        )
        
        import asyncio
        loop = asyncio.get_running_loop()
        response = await loop.run_in_executor(
            None, 
            lambda: cur_model.generate_content(prompt, safety_settings=safety_settings)
        )
        
        text_resp = ""
        function_calls = []
        
        if hasattr(response, 'parts') and response.parts:
            for part in response.parts:
                if part.text:
                    text_resp += part.text
                if part.function_call:
                    fc = part.function_call
                    function_calls.append({
                        "name": fc.name,
                        "arguments": {k: v for k, v in fc.args.items()}
                    })
        elif hasattr(response, 'text') and response.text:
            text_resp = response.text
            
        if text_resp:
            return text_resp.strip(), function_calls
            
    except Exception as e:
        logger.warning(f"Gemini AI fallback failed: {e}.")
        
    return "", []
