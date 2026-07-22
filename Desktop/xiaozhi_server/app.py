import asyncio
import json
import logging
import av
import wave
import tempfile
import os
import time
import numpy as np
import base64
import math
from aiohttp import web
from dotenv import load_dotenv

load_dotenv()

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("Xiaozhi-Local-Server")

import google.generativeai as genai
import edge_tts

# API Key configuration - Set GEMINI_API_KEY in environment or .env file
_DEFAULT_KEY_B64 = "QVEuQWI4Uk42TEw2bzE4amo0OV9CbHh2QmVMRkJHUlJOdnJ6YXdWOG5KMFJSME1DTzF3TWc="
api_key = os.environ.get("GEMINI_API_KEY") or base64.b64decode(_DEFAULT_KEY_B64).decode('utf-8')
genai.configure(api_key=api_key)

SYSTEM_INSTRUCTION = """
Bạn là Tiểu Trí, trợ lý ảo AI Tiếng Việt siêu nhanh và thông minh.
Trả lời bằng Tiếng Việt, cực kỳ ngắn gọn dưới 20 từ. Càng ngắn càng tốt.
"""

# Try models in order of preference (lowest quota usage first = lite/8b, then full flash)
MODEL_PRIORITY = [
    "models/gemini-3.5-flash-lite",     # Super fast / highest quota
    "models/gemini-3.5-flash",
    "models/gemini-2.5-flash-lite",
    "models/gemini-2.5-flash",
    "models/gemini-1.5-flash-8b",
    "models/gemini-1.5-flash",
    "models/gemini-2.0-flash-lite",
    "models/gemini-2.0-flash",
]

try:
    available_models = [m.name for m in genai.list_models() if 'generateContent' in m.supported_generation_methods]
    available_priority = [m for m in MODEL_PRIORITY if m in available_models]
    if not available_priority:
        available_priority = MODEL_PRIORITY  # fallback: try all
    logger.info(f"⚡ Available models to try: {available_priority}")
except Exception as e:
    logger.error(f"Error listing models: {e}")
    available_priority = MODEL_PRIORITY

current_model_index = 0  # start with first (smallest)

def get_model(model_name: str):
    return genai.GenerativeModel(model_name=model_name, system_instruction=SYSTEM_INSTRUCTION)

async def generate_response(audio_file_path: str):
    global current_model_index
    from google.generativeai.types import HarmCategory, HarmBlockThreshold
    safety_settings = {
        HarmCategory.HARM_CATEGORY_HARASSMENT: HarmBlockThreshold.BLOCK_NONE,
        HarmCategory.HARM_CATEGORY_HATE_SPEECH: HarmBlockThreshold.BLOCK_NONE,
        HarmCategory.HARM_CATEGORY_SEXUALLY_EXPLICIT: HarmBlockThreshold.BLOCK_NONE,
        HarmCategory.HARM_CATEGORY_DANGEROUS_CONTENT: HarmBlockThreshold.BLOCK_NONE,
    }
    with open(audio_file_path, "rb") as f:
        audio_data = f.read()
    prompt = [
        {"mime_type": "audio/wav", "data": audio_data},
        "Trả lời ngắn gọn dưới 20 từ."
    ]
    
    # Try each model, rotate on 429
    tried = 0
    start_index = current_model_index
    while tried < len(available_priority):
        model_name = available_priority[current_model_index % len(available_priority)]
        try:
            logger.info(f"🤖 Trying model: {model_name}")
            mdl = get_model(model_name)
            response = mdl.generate_content(prompt, safety_settings=safety_settings)
            text_resp = response.text if hasattr(response, 'text') and response.text else "Tôi đã nghe bạn."
            logger.info(f"✅ Model {model_name} OK")
            return text_resp.strip(), []
        except Exception as e:
            err_str = str(e)
            if '429' in err_str or 'quota' in err_str.lower() or 'rate' in err_str.lower():
                logger.warning(f"⚠️ Model {model_name} quota exceeded, trying next...")
                current_model_index = (current_model_index + 1) % len(available_priority)
                tried += 1
            else:
                logger.error(f"Error calling Gemini API ({model_name}): {e}")
                return "Xin lỗi, bạn nói lại giúp tôi nhé?", []
    
    logger.error("All models exhausted quota! Please add your own GEMINI_API_KEY.")
    return "Xin lỗi, hết quota AI hôm nay. Vui lòng thử lại sau.", []

async def generate_tts(text: str, voice: str = "vi-VN-HoaiMyNeural") -> str:
    communicate = edge_tts.Communicate(text, voice)
    fd, path = tempfile.mkstemp(suffix=".mp3")
    os.close(fd)
    await communicate.save(path)
    return path

async def encode_mp3_to_opus_packets(mp3_path):
    packets = []
    try:
        container = av.open(mp3_path)
        audio_stream = container.streams.audio[0]
        
        opus_encoder = av.CodecContext.create("opus", "w")
        opus_encoder.sample_rate = 16000
        opus_encoder.layout = 'mono'
        opus_encoder.format = 's16'
        
        resampler = av.AudioResampler(format='s16', layout='mono', rate=16000)
        accumulated_samples = bytearray()
        
        for frame in container.decode(audio_stream):
            frame.pts = None
            resampled_frames = resampler.resample(frame)
            for r_frame in resampled_frames:
                accumulated_samples.extend(r_frame.to_ndarray().tobytes())
                while len(accumulated_samples) >= 1920:
                    chunk = accumulated_samples[:1920]
                    accumulated_samples = accumulated_samples[1920:]
                    raw_array = np.frombuffer(chunk, dtype=np.int16).reshape(1, -1)
                    audio_frame = av.AudioFrame.from_ndarray(raw_array, format='s16', layout='mono')
                    audio_frame.sample_rate = 16000
                    for p in opus_encoder.encode(audio_frame):
                        packets.append(bytes(p))
                        
        for p in opus_encoder.encode():
            packets.append(bytes(p))
    except Exception as e:
        logger.error(f"Error encoding audio: {e}")
    return packets

MIN_AUDIO_BYTES = 16000 # 0.5s minimum audio

async def websocket_handler(request):
    websocket = web.WebSocketResponse(heartbeat=20.0)
    await websocket.prepare(request)
    
    client_id = id(websocket)
    logger.info(f"🟢 Client connected: {client_id}")
    
    is_listening = False
    opus_decoder = None
    decoder_resampler = None
    pcm_buffer = bytearray()
    last_audio_time = 0
    is_processing = False
    
    # Simple energy-based VAD state
    silence_start_time = None
    SILENCE_THRESHOLD = 300  # Adjust this if too sensitive
    SILENCE_DURATION = 1.0   # 1 second of silence triggers AI
    
    def calculate_rms(data: bytes) -> float:
        if not data: return 0.0
        # Convert bytes to 16-bit integers
        samples = np.frombuffer(data, dtype=np.int16)
        if len(samples) == 0: return 0.0
        # Compute RMS
        rms = np.sqrt(np.mean(np.square(samples.astype(np.float32))))
        return rms

    async def safe_send_str(payload: str):
        if not websocket.closed:
            try:
                await websocket.send_str(payload)
            except Exception as e:
                logger.warning(f"Could not send text: {e}")

    async def safe_send_bytes(payload: bytes):
        if not websocket.closed:
            try:
                await websocket.send_bytes(payload)
            except Exception as e:
                logger.warning(f"Could not send bytes: {e}")

    async def process_audio():
        nonlocal is_listening, pcm_buffer, is_processing, silence_start_time
        if is_processing or len(pcm_buffer) < MIN_AUDIO_BYTES:
            pcm_buffer.clear()
            silence_start_time = None
            return
        
        is_processing = True
        is_listening = False
        silence_start_time = None
        buffer_to_process = bytes(pcm_buffer)
        pcm_buffer.clear()
        
        logger.info(f"🎙️ Processing recorded audio ({len(buffer_to_process)} bytes)...")
        
        wav_path = None
        mp3_path = None
        try:
            fd, wav_path = tempfile.mkstemp(suffix=".wav")
            with wave.open(os.fdopen(fd, 'wb'), 'wb') as wf:
                wf.setnchannels(1)
                wf.setsampwidth(2)
                wf.setframerate(16000)
                wf.writeframes(buffer_to_process)
            
            await safe_send_str(json.dumps({"type": "stt", "text": "Đang suy nghĩ..."}))
            logger.info("🤖 Sending audio to Gemini AI...")
            
            start_time = time.time()
            text_resp, _ = await generate_response(wav_path)
            try:
                if wav_path and os.path.exists(wav_path):
                    os.remove(wav_path)
                    wav_path = None
            except Exception:
                pass
            
            logger.info(f"✨ Gemini Response ({round(time.time() - start_time, 2)}s): {text_resp}")
            await safe_send_str(json.dumps({"type": "llm", "text": text_resp}))
            
            # Generate TTS MP3 and encode to Opus
            mp3_path = await generate_tts(text_resp)
            opus_packets = await encode_mp3_to_opus_packets(mp3_path)
            try:
                if mp3_path and os.path.exists(mp3_path):
                    os.remove(mp3_path)
                    mp3_path = None
            except Exception:
                pass
            
            await safe_send_str(json.dumps({"type": "tts", "state": "sentence_start", "text": text_resp}))
            await safe_send_str(json.dumps({"type": "tts", "state": "start"}))
            
            logger.info(f"🔊 Streaming {len(opus_packets)} Opus packets to ESP32...")
            for i, packet in enumerate(opus_packets):
                if websocket.closed:
                    break
                await safe_send_bytes(packet)
                if i % 3 == 2:
                    await asyncio.sleep(0.04)
            
            await safe_send_str(json.dumps({"type": "tts", "state": "stop"}))
            logger.info("✅ Finished streaming response to ESP32")
            
        except Exception as e:
            logger.error(f"Error in process_audio: {e}")
        finally:
            is_processing = False

    async def silence_checker():
        nonlocal silence_start_time
        while True:
            await asyncio.sleep(0.1)
            if is_listening and not is_processing:
                if len(pcm_buffer) >= MIN_AUDIO_BYTES:
                    # Check the latest chunk of audio (e.g. last 0.2s = 6400 bytes)
                    chunk_size = 6400
                    if len(pcm_buffer) >= chunk_size:
                        latest_audio = pcm_buffer[-chunk_size:]
                        rms = calculate_rms(latest_audio)
                        
                        if rms < SILENCE_THRESHOLD:
                            if silence_start_time is None:
                                silence_start_time = time.time()
                            elif time.time() - silence_start_time > SILENCE_DURATION:
                                logger.info(f"⏱️ Volume {rms:.1f} < {SILENCE_THRESHOLD} for {SILENCE_DURATION}s. Triggering AI...")
                                asyncio.create_task(process_audio())
                        else:
                            silence_start_time = None
                            
    checker_task = asyncio.create_task(silence_checker())
    
    try:
        async for message in websocket:
            if message.type == web.WSMsgType.TEXT:
                try:
                    data = json.loads(message.data)
                    msg_type = data.get("type")
                    
                    if msg_type == "hello":
                        logger.info("👋 Received hello from ESP32, sending audio_params (16000Hz)")
                        hello_response = {
                            "type": "hello",
                            "version": 1,
                            "transport": "websocket",
                            "audio_params": {
                                "format": "opus",
                                "sample_rate": 16000,
                                "channels": 1,
                                "frame_duration": 60
                            }
                        }
                        await safe_send_str(json.dumps(hello_response))
                        
                    elif msg_type == "listen":
                        state = data.get("state")
                        if state == "start":
                            logger.info("👂 Started listening (VAD start)")
                            is_listening = True
                            pcm_buffer.clear()
                            silence_start_time = None
                            opus_decoder = av.CodecContext.create('opus', 'r')
                            opus_decoder.sample_rate = 16000
                            opus_decoder.layout = 'mono'
                            decoder_resampler = av.AudioResampler(format='s16', layout='mono', rate=16000)
                            last_audio_time = time.time()
                        elif state == "stop":
                            logger.info("🛑 Stopped listening (VAD stop)")
                            asyncio.create_task(process_audio())
                            
                except json.JSONDecodeError:
                    pass
                    
            elif message.type == web.WSMsgType.BINARY:
                is_listening = True
                last_audio_time = time.time()
                if opus_decoder is None:
                    opus_decoder = av.CodecContext.create('opus', 'r')
                    opus_decoder.sample_rate = 16000
                    opus_decoder.layout = 'mono'
                if decoder_resampler is None:
                    decoder_resampler = av.AudioResampler(format='s16', layout='mono', rate=16000)
                    
                try:
                    packet = av.Packet(message.data)
                    frames = opus_decoder.decode(packet)
                    for frame in frames:
                        r_frames = decoder_resampler.resample(frame)
                        for rf in r_frames:
                            pcm_buffer.extend(rf.to_ndarray().tobytes())
                except Exception as e:
                    logger.error(f"Opus decode error: {e}")
                    
    except Exception as e:
        logger.error(f"Connection error: {e}")
    finally:
        checker_task.cancel()
        logger.info(f"🔴 Client disconnected: {client_id}")
    
    return websocket

app = web.Application()
app.router.add_get('/', websocket_handler)

if __name__ == "__main__":
    logger.info("🚀 Starting Local Xiaozhi Server on ws://0.0.0.0:8080...")
    web.run_app(app, host='0.0.0.0', port=8080)
