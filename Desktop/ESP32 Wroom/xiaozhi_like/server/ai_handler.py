import google.generativeai as genai
import os
import logging
import base64
from dotenv import load_dotenv

load_dotenv()

logger = logging.getLogger("Xiaozhi-Server")

# Encoded fallback API key
_DEFAULT_KEY_B64 = "QVEuQWI4Uk42TEw2bzE4amo0OV9CbHh2QmVMRkJHUlJOdnJ6YXdWOG5KMFJSME1DTzF3TWc="

def get_api_key():
    env_key = os.environ.get("GEMINI_API_KEY")
    if env_key and len(env_key) > 5:
        return env_key
    try:
        return base64.b64decode(_DEFAULT_KEY_B64).decode('utf-8')
    except Exception:
        return ""

api_key = get_api_key()
if api_key:
    genai.configure(api_key=api_key)

SYSTEM_INSTRUCTION = """
Bạn là Tiểu Trí, một trợ lý ảo AI thông minh, thân thiện, trả lời ngắn gọn và hữu ích.
Bạn giao tiếp tự nhiên bằng Tiếng Việt. Trả lời cực ngắn gọn (dưới 40 chữ).
"""

def control_led(state: str):
    """Control the LED light on the device."""
    pass

def stop_conversation():
    """Call this tool to stop the continuous conversation."""
    pass

tools = [control_led, stop_conversation]

def get_best_model():
    """Auto-detect the best available Gemini model"""
    try:
        available_models = [m.name for m in genai.list_models() if 'generateContent' in m.supported_generation_methods]
        logger.info(f"Available Gemini models: {available_models}")
        
        preferred = [
            "models/gemini-2.5-flash",
            "models/gemini-2.0-flash",
            "models/gemini-1.5-flash",
            "models/gemini-flash-latest",
            "models/gemini-2.5-flash-lite",
            "models/gemini-3.5-flash"
        ]
        
        for p in preferred:
            if p in available_models:
                logger.info(f"✅ Auto-selected Gemini Model: {p}")
                return p
                
        for m in available_models:
            if "flash" in m.lower():
                logger.info(f"✅ Auto-selected Gemini Model: {m}")
                return m
                
        if available_models:
            logger.info(f"✅ Auto-selected Gemini Model: {available_models[0]}")
            return available_models[0]
            
    except Exception as e:
        logger.warning(f"Could not auto-detect Gemini models ({e}). Using default models/gemini-2.5-flash.")
        
    return "models/gemini-2.5-flash"

try:
    model_name = get_best_model()
    model = genai.GenerativeModel(
        model_name=model_name,
        tools=tools,
        system_instruction=SYSTEM_INSTRUCTION
    )
except Exception as e:
    logger.error(f"Failed to create GenerativeModel: {e}")
    model = genai.GenerativeModel(model_name="models/gemini-2.5-flash")

async def generate_response(audio_file_path: str):
    """
    Send audio to Gemini and get text + function calls.
    """
    try:
        with open(audio_file_path, "rb") as f:
            audio_data = f.read()
            
        prompt = [
            {"mime_type": "audio/wav", "data": audio_data},
            "Trả lời bằng tiếng Việt, cực kỳ ngắn gọn dưới 30 từ."
        ]
        
        from google.generativeai.types import HarmCategory, HarmBlockThreshold
        safety_settings = {
            HarmCategory.HARM_CATEGORY_HARASSMENT: HarmBlockThreshold.BLOCK_NONE,
            HarmCategory.HARM_CATEGORY_HATE_SPEECH: HarmBlockThreshold.BLOCK_NONE,
            HarmCategory.HARM_CATEGORY_SEXUALLY_EXPLICIT: HarmBlockThreshold.BLOCK_NONE,
            HarmCategory.HARM_CATEGORY_DANGEROUS_CONTENT: HarmBlockThreshold.BLOCK_NONE,
        }
        
        response = model.generate_content(prompt, safety_settings=safety_settings)
        
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
            
        if not text_resp:
            text_resp = "Tôi đã nghe bạn nói."
            
        return text_resp, function_calls
        
    except Exception as e:
        logger.error(f"Error calling Gemini API: {e}", exc_info=True)
        return "Xin lỗi, tôi không nghe rõ. Bạn nói lại được không?", []
