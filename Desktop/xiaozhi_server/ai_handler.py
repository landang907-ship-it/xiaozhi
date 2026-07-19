import google.generativeai as genai
import os
from dotenv import load_dotenv

load_dotenv()

# Initialize Gemini
api_key = os.environ.get("GEMINI_API_KEY")
if api_key:
    genai.configure(api_key=api_key)

# Basic tools that the AI can call
def control_led(state: str):
    """
    Control the LED light on the device.
    
    Args:
        state: "on" to turn the LED on, "off" to turn it off.
    """
    pass # Real logic is sent to ESP32

tools = [control_led]

# System instruction for the voice assistant
SYSTEM_INSTRUCTION = """
Bạn là trợ lý ảo AI thông minh, tên là Xiaozhi (Tiểu Trí), được thiết kế bởi nhóm phát triển.
Bạn có thể trò chuyện tự nhiên với người dùng bằng Tiếng Việt.
Khi trả lời, hãy trả lời ngắn gọn, thân thiện, súc tích (dưới 40 chữ).
Bạn có thể điều khiển thiết bị thông qua các function/tools (ví dụ: bật tắt đèn LED).
"""

def get_best_model():
    """Auto-detect the best free/fast Gemini model available"""
    try:
        available_models = [m.name for m in genai.list_models() if 'generateContent' in m.supported_generation_methods]
        
        # Prioritize flash models (fast & usually have free tier)
        preferred = [
            "models/gemini-2.5-flash",
            "models/gemini-flash-latest",
            "models/gemini-3.5-flash",
            "models/gemini-2.5-flash-lite",
            "models/gemini-pro-latest"
        ]
        
        for p in preferred:
            if p in available_models:
                print(f"✅ Auto-selected API Model: {p}")
                return p
                
        # Fallback to any model with 'flash'
        for m in available_models:
            if "flash" in m.lower():
                print(f"✅ Auto-selected API Model: {m}")
                return m
                
        if available_models:
            print(f"✅ Auto-selected API Model: {available_models[0]}")
            return available_models[0]
            
    except Exception as e:
        print(f"⚠️ Could not auto-detect models ({e}). Using default.")
        
    return "gemini-2.5-flash"

model = genai.GenerativeModel(
    model_name=get_best_model(),
    tools=tools,
    system_instruction=SYSTEM_INSTRUCTION
)

async def generate_response(audio_file_path: str):
    """
    Send audio to Gemini 1.5 and get text + function calls.
    Returns: (text_response, function_calls_list)
    """
    # Upload the file to Gemini
    # For quick responses with small audio, inline data is better, but the SDK requires a file upload for audio usually,
    # or passing raw bytes with mime_type.
    
    try:
        # We can pass raw bytes
        with open(audio_file_path, "rb") as f:
            audio_data = f.read()
            
        prompt = [
            {"mime_type": "audio/wav", "data": audio_data},
            "Trả lời ngắn gọn."
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
        
        for part in response.parts:
            if part.text:
                text_resp += part.text
            if part.function_call:
                fc = part.function_call
                function_calls.append({
                    "name": fc.name,
                    "arguments": {k: v for k, v in fc.args.items()}
                })
                
        return text_resp, function_calls
        
    except Exception as e:
        print(f"Error in Gemini: {e}")
        return "Xin lỗi, tôi gặp lỗi khi xử lý âm thanh.", []
