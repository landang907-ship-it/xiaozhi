#!/usr/bin/env python3
"""
P8 Server - ASR + LLM + TTS WebSocket Server
For Mini-Xiaozhi ESP32-S3
"""

import asyncio
import json
import logging
import os
import sys
import time
import io
import wave

import numpy as np

# Add current directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import websockets
from config import (
    HOST, PORT, SAMPLE_RATE, AUDIO_CHANNELS,
    DEFAULT_LLM, DEFAULT_ASR, DEFAULT_TTS,
    OLLAMA_URL, OLLAMA_MODEL, OPENAI_MODEL, OPENAI_API_KEY, OPENAI_BASE_URL,
    WHISPER_MODEL, SYSTEM_PROMPT
)

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s"
)
logger = logging.getLogger(__name__)

# ─── Audio constants ───────────────────────────────────────────────────────────
ESP32_SAMPLE_RATE   = 24000   # ESP32 INMP441 sample rate
WHISPER_SAMPLE_RATE = 16000   # Whisper expected sample rate
SILENCE_TIMEOUT_S   = 1.5     # Seconds of silence before processing
MIN_AUDIO_BYTES     = ESP32_SAMPLE_RATE * 1     # Min 1 second of u8 audio
MAX_AUDIO_BYTES     = ESP32_SAMPLE_RATE * 8     # Max 8 second buffer


def u8_24k_to_float32_16k(u8_data: bytes) -> np.ndarray:
    """
    Convert ESP32 audio format to Whisper-compatible float32 @ 16kHz.

    ESP32 encoding (ws_send_audio):
        uint8_t u8 = (int16_t >> 8) + 128;
    Reverse:
        int16 = (u8 - 128) << 8
    Then resample 24000 → 16000 and normalize to [-1.0, 1.0].
    """
    u8_array = np.frombuffer(u8_data, dtype=np.uint8).astype(np.int32)
    int16_array = ((u8_array - 128) << 8).clip(-32768, 32767).astype(np.int16)

    # Resample 24kHz → 16kHz
    try:
        from scipy.signal import resample_poly
        resampled = resample_poly(int16_array, 2, 3)  # 24000 * 2/3 = 16000
    except ImportError:
        # Pure-numpy fallback: linear interpolation
        old_len = len(int16_array)
        new_len = int(old_len * WHISPER_SAMPLE_RATE / ESP32_SAMPLE_RATE)
        indices = np.linspace(0, old_len - 1, new_len)
        resampled = np.interp(indices, np.arange(old_len),
                              int16_array.astype(np.float64))

    return resampled.astype(np.float32) / 32768.0


def build_wav(u8_data: bytes) -> bytes:
    """Build a 16-bit 16kHz WAV from u8 24kHz data (for file-based ASR)."""
    float32 = u8_24k_to_float32_16k(u8_data)
    int16 = (float32 * 32767).clip(-32768, 32767).astype(np.int16)
    buf = io.BytesIO()
    with wave.open(buf, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(WHISPER_SAMPLE_RATE)
        wf.writeframes(int16.tobytes())
    return buf.getvalue()


class AIServer:
    def __init__(self):
        self.asr   = None
        self.llm   = None
        self.tts   = None

    # ── Init ──────────────────────────────────────────────────────────────────

    async def init_asr(self):
        if DEFAULT_ASR == "none":
            self.asr = None
            logger.info("ASR: disabled (echo/silence mode)")
        elif DEFAULT_ASR == "whisper":
            try:
                from faster_whisper import WhisperModel
                self.asr = WhisperModel(WHISPER_MODEL, device="cpu", compute_type="int8")
                logger.info(f"Faster-Whisper ASR loaded (model={WHISPER_MODEL})")
            except Exception as e:
                logger.error(f"Faster-Whisper load failed: {e}")
                self.asr = None
        elif DEFAULT_ASR == "silero":
            try:
                import silero
                self.asr = silero.load_model()
                logger.info("Silero ASR loaded")
            except Exception as e:
                logger.error(f"Silero load failed: {e}")
                self.asr = None

    async def init_llm(self):
        if DEFAULT_LLM == "ollama":
            self.llm = {"type": "ollama", "url": OLLAMA_URL, "model": OLLAMA_MODEL}
            logger.info(f"Ollama LLM: {OLLAMA_URL} model={OLLAMA_MODEL}")
        elif DEFAULT_LLM == "openai" and OPENAI_API_KEY:
            self.llm = {
                "type": "openai", 
                "model": OPENAI_MODEL, 
                "api_key": OPENAI_API_KEY, 
                "base_url": OPENAI_BASE_URL
            }
            logger.info(f"OpenAI LLM: {OPENAI_MODEL} (Base URL: {OPENAI_BASE_URL})")
        else:
            self.llm = None
            logger.warning("LLM not configured. Set DEFAULT_LLM=ollama or OPENAI_API_KEY.")

    async def init_tts(self):
        if DEFAULT_TTS == "none":
            self.tts = None
            logger.info("TTS: disabled")
        elif DEFAULT_TTS == "edge":
            try:
                import edge_tts  # noqa: F401
                self.tts = {"type": "edge", "voice": "vi-VN-HoaiMyNeural"}
                logger.info("Edge TTS configured (Vietnamese)")
            except ImportError:
                logger.warning("edge-tts not installed.")
                self.tts = None

    # ── AI pipeline ───────────────────────────────────────────────────────────

    async def asr_transcribe(self, audio_data: bytes) -> str:
        """Transcribe buffered u8 24kHz audio."""
        if not self.asr:
            return ""
        try:
            float32_audio = u8_24k_to_float32_16k(audio_data)

            # ── Normalize peak to 0.9 to fix clipping distortion ─────────────
            peak_raw = float(np.max(np.abs(float32_audio)))
            if peak_raw > 0.01:  # Only normalize if there's actual signal
                float32_audio = float32_audio * (0.9 / peak_raw)

            # No noise gate - Whisper handles silence perfectly on its own
            # ── Audio level diagnostic ────────────────────────────────────────
            rms = float(np.sqrt(np.mean(float32_audio ** 2)))
            peak = float(np.max(np.abs(float32_audio)))
            duration = len(float32_audio) / WHISPER_SAMPLE_RATE
            logger.info(
                f"Transcribing {len(audio_data)} bytes "
                f"({duration:.1f}s @ 16kHz) | RMS={rms:.4f} Peak={peak:.4f}"
            )
            if rms < 0.001:
                logger.warning("Audio level very low (RMS<0.001) — mic may be silent or gain too low")

            # Run blocking Whisper in a thread so we don't block the event loop
            loop = asyncio.get_event_loop()
            def do_transcribe():
                segments, info = self.asr.transcribe(
                    float32_audio,
                    language="vi",
                    condition_on_previous_text=False,
                    beam_size=5,
                )
                return " ".join([segment.text for segment in segments]).strip(), info

            text, info = await loop.run_in_executor(None, do_transcribe)
            logger.info(f"ASR raw: '{text}' (prob={info.language_probability:.3f})")
            return text
        except Exception as e:
            logger.error(f"ASR error: {e}")
            return ""


    async def llm_generate_stream(self, prompt: str = None, wav_bytes: bytes = None):
        """Yield response chunks from LLM (Ollama or Gemini)."""
        if not self.llm:
            yield "LLM not configured."
            return
            
        in_think_block = False
        buffer = ""
        
        def process_chunk(content):
            nonlocal in_think_block, buffer
            buffer += content
            
            if not in_think_block and "<think>" in buffer:
                pre_think, rest = buffer.split("<think>", 1)
                buffer = rest
                in_think_block = True
                if pre_think:
                    return pre_think
                    
            if in_think_block and "</think>" in buffer:
                _, post_think = buffer.split("</think>", 1)
                buffer = post_think
                in_think_block = False
                
            if in_think_block:
                return ""
                
            safe_to_yield = buffer
            for i in range(1, 8):
                if buffer.endswith("<think>"[:i]):
                    safe_to_yield = buffer[:-i]
                    buffer = buffer[-i:]
                    break
            else:
                buffer = ""
            return safe_to_yield

        try:
            if self.llm["type"] == "openai":
                import aiohttp
                import base64
                
                url = f"https://generativelanguage.googleapis.com/v1beta/models/{self.llm['model']}:streamGenerateContent?key={self.llm['api_key']}&alt=sse"
                
                parts = []
                if wav_bytes:
                    b64_audio = base64.b64encode(wav_bytes).decode('utf-8')
                    parts.append({
                        "inlineData": {
                            "mimeType": "audio/wav",
                            "data": b64_audio
                        }
                    })
                    # Prompt accompanying the audio
                    if prompt:
                        parts.append({"text": prompt})
                    else:
                        parts.append({"text": "Hãy đóng vai Tiểu Trí, một trợ lý ảo giao tiếp bằng giọng nói. Trả lời tự nhiên, ngắn gọn yêu cầu của tôi. KHÔNG ĐƯỢC lặp lại câu hỏi hay nhắc đến việc đây là một 'đoạn ghi âm'."})
                elif prompt:
                    parts.append({"text": prompt})
                    
                payload = {
                    "contents": [{"parts": parts}],
                    "systemInstruction": {"parts": [{"text": SYSTEM_PROMPT}]}
                }
                async with aiohttp.ClientSession() as session:
                    async with session.post(url, json=payload, timeout=aiohttp.ClientTimeout(total=60)) as resp:
                        if resp.status != 200:
                            error_text = await resp.text()
                            raise Exception(f"Gemini API Error {resp.status}: {error_text}")
                            
                        import json
                        async for line in resp.content:
                            line = line.decode('utf-8').strip()
                            if line.startswith("data: "):
                                try:
                                    data_json = json.loads(line[6:])
                                    text_val = data_json["candidates"][0]["content"]["parts"][0]["text"]
                                    text = process_chunk(text_val)
                                    if text:
                                        yield text
                                except:
                                    pass
                text = process_chunk("")
                if text:
                    yield text
            elif self.llm["type"] == "ollama":
                # Basic non-streaming fallback for ollama if used
                import aiohttp
                async with aiohttp.ClientSession() as session:
                    async with session.post(
                        f"{self.llm['url']}/api/generate",
                        json={"model": self.llm["model"], "prompt": prompt, "stream": False},
                        timeout=aiohttp.ClientTimeout(total=30)
                    ) as resp:
                        if resp.status == 200:
                            data = await resp.json()
                            yield data.get("response", "").strip()
        except Exception as e:
            logger.error(f"LLM error: {e}")
            yield "Xin lỗi, máy chủ AI đang gặp sự cố kết nối, vui lòng thử lại sau."

    async def tts_speak(self, text: str) -> bytes:
        if not self.tts or not text:
            return b""
        try:
            if self.tts["type"] == "edge":
                import edge_tts
                
                mp3_bytes = b""
                for attempt in range(3):
                    try:
                        async def fetch_tts():
                            communicate = edge_tts.Communicate(text, self.tts["voice"])
                            chunks = []
                            async for chunk in communicate.stream():
                                if chunk["type"] == "audio":
                                    chunks.append(chunk["data"])
                            return b"".join(chunks)
                            
                        # Set a strict 5-second timeout to prevent Microsoft server hangs
                        mp3_bytes = await asyncio.wait_for(fetch_tts(), timeout=5.0)
                        if mp3_bytes:
                            break
                    except Exception as err:
                        if attempt < 2:
                            await asyncio.sleep(0.1)
                        else:
                            raise err
                            
                if not mp3_bytes:
                    return b""

                # Decode MP3 to 24kHz mono 16-bit PCM using miniaudio
                import miniaudio
                decoded = miniaudio.decode(
                    mp3_bytes,
                    output_format=miniaudio.SampleFormat.SIGNED16,
                    nchannels=1,
                    sample_rate=24000
                )

                # Convert to unsigned 8-bit PCM (s16_sample >> 8) + 128
                import numpy as np
                s16_array = np.frombuffer(decoded.samples, dtype=np.int16)
                u8_array = ((s16_array.astype(np.int32) >> 8) + 128).clip(0, 255).astype(np.uint8)
                logger.info(f"Generated TTS speech audio: {len(u8_array)} bytes (24kHz Mono u8)")
                return u8_array.tobytes()
        except Exception as e:
            logger.error(f"TTS error: {e}")
            return b""

    # ── Audio processing pipeline ─────────────────────────────────────────────

    async def _process_audio_buffer(self, websocket, audio_buffer: bytearray):
        """
        Process a complete utterance:
        1. Signal ESP32 we are thinking (→ stops mic on device)
        2. ASR → LLM Stream → TTS Chunked Streaming
        3. Send result or reset to listening
        """
        logger.info(
            f"Processing audio buffer: {len(audio_buffer)} bytes "
            f"({len(audio_buffer)/ESP32_SAMPLE_RATE:.1f}s)"
        )

        # ── Save WAV and Raw binary for diagnostic ────────────────────────────
        try:
            timestamp = int(time.time())
            raw_path = f"debug_raw_{timestamp}.bin"
            with open(raw_path, "wb") as f:
                f.write(bytes(audio_buffer))
            logger.info(f"Saved debug RAW: {raw_path} ({len(audio_buffer)} bytes)")

            wav_bytes = build_wav(bytes(audio_buffer))
            wav_path = f"debug_audio_{timestamp}.wav"
            with open(wav_path, "wb") as f:
                f.write(wav_bytes)
            logger.info(f"Saved debug WAV: {wav_path} ({len(wav_bytes)} bytes)")
        except Exception as e:
            logger.warning(f"Could not save debug files: {e}")

        # Stop mic on device
        await websocket.send(json.dumps({"action": "thinking"}))
        
        # Send placeholder text to ESP32 screen
        await websocket.send(json.dumps({"type": "asr_result", "text": "🎤 Đang gửi âm thanh cho Gemini..."}))
        
        logger.info("Starting LLM stream (End-to-End Audio) and TTS chunks...")
        await websocket.send(json.dumps({"action": "startSpeaking"}))
        
        audio_queue = asyncio.Queue()
        text_queue = asyncio.Queue()
        
        async def tts_worker():
            while True:
                sentence = await text_queue.get()
                if sentence is None:
                    await audio_queue.put(None)
                    break
                logger.info(f"TTS Chunk: '{sentence}'")
                await websocket.send(json.dumps({"type": "llm_response", "text": sentence + " "}))
                
                tts_audio = await self.tts_speak(sentence)
                if tts_audio:
                    await audio_queue.put(tts_audio)
                text_queue.task_done()
                
        tts_task = asyncio.create_task(tts_worker())
        
        async def audio_pacer():
            start_time = time.time()
            bytes_sent = 0
            
            while True:
                audio_bytes = await audio_queue.get()
                if audio_bytes is None:
                    break
                
                chunk_size = 4000
                for i in range(0, len(audio_bytes), chunk_size):
                    chunk = audio_bytes[i:i+chunk_size]
                    try:
                        await websocket.send(chunk)
                        bytes_sent += len(chunk)
                        
                        while True:
                            elapsed = time.time() - start_time
                            bytes_played = elapsed * 24000
                            buffer_level = bytes_sent - bytes_played
                            
                            # Prevent debt accumulation if generation was slow
                            if buffer_level < 0:
                                start_time = time.time() - (bytes_sent / 24000.0)
                                buffer_level = 0
                            
                            if buffer_level <= 16000:
                                break
                            await asyncio.sleep(0.02)
                    except Exception:
                        break
                        
                audio_queue.task_done()

        pacer_task = asyncio.create_task(audio_pacer())
        
        sentence_buffer = ""
        delimiters = {'.', ',', '?', '!', '\n', ':', ';'}
        
        # Bypass ASR! Send raw WAV bytes directly to Gemini!
        async for text_chunk in self.llm_generate_stream(wav_bytes=wav_bytes):
            sentence_buffer += text_chunk
            
            while True:
                first_delim_idx = -1
                for i in range(len(sentence_buffer)):
                    if sentence_buffer[i] in delimiters:
                        first_delim_idx = i
                        break
                        
                if first_delim_idx != -1:
                    sentence = sentence_buffer[:first_delim_idx+1].strip()
                    sentence_buffer = sentence_buffer[first_delim_idx+1:]
                    
                    if len(sentence) > 1:
                        await text_queue.put(sentence)
                else:
                    break
        
        # Flush remaining text
        sentence = sentence_buffer.strip()
        if sentence:
            await text_queue.put(sentence)
            
        await text_queue.put(None)
        await tts_task
        await pacer_task

        await websocket.send(json.dumps({"action": "stopSpeaking"}))
        return

        # No meaningful output — go straight back to listening
        await websocket.send(json.dumps({"action": "stopSpeaking"}))

    # ── Connection handler ────────────────────────────────────────────────────

    async def handle_message(self, request):
        """Handle a single WebSocket client connection."""
        import aiohttp
        from aiohttp import web
        websocket = web.WebSocketResponse()
        await websocket.prepare(request)
        
        logger.info("Client connected")
        audio_buffer = bytearray()

        # Helper to mimic old websockets send API
        class WSWrap:
            async def send(self, data):
                if isinstance(data, str):
                    await websocket.send_str(data)
                elif isinstance(data, bytes):
                    await websocket.send_bytes(data)

        ws_wrap = WSWrap()

        # Tell device to start sending audio
        await ws_wrap.send(json.dumps({"action": "listening"}))

        try:
            while True:
                try:
                    # Wait with timeout — expiry = silence detected
                    msg = await asyncio.wait_for(
                        websocket.receive(), timeout=SILENCE_TIMEOUT_S
                    )

                    if msg.type == aiohttp.WSMsgType.BINARY:
                        audio_buffer.extend(msg.data)

                        # Force-process if buffer is too large
                        if len(audio_buffer) >= MAX_AUDIO_BYTES:
                            logger.info("Buffer full — forcing process")
                            await self._process_audio_buffer(ws_wrap, audio_buffer)
                            audio_buffer.clear()
                            # Back to listening after processing
                            await ws_wrap.send(json.dumps({"action": "listening"}))

                    elif msg.type == aiohttp.WSMsgType.TEXT:
                        # Text / JSON command from device
                        try:
                            data = json.loads(msg.data)
                        except Exception:
                            logger.warning(f"Non-JSON text: {msg.data!r}")
                            continue

                        cmd = data.get("cmd")
                        if data.get("state") == "stop_capture":
                            if len(audio_buffer) >= MIN_AUDIO_BYTES:
                                logger.info("Received stop_capture signal. Processing immediately.")
                                await self._process_audio_buffer(ws_wrap, audio_buffer)
                                audio_buffer.clear()
                                await ws_wrap.send(json.dumps({"action": "listening"}))
                            continue

                        if cmd == "ping":
                            await ws_wrap.send(json.dumps({"type": "pong"}))
                        elif cmd == "tts_speak":
                            audio = await self.tts_speak(data.get("text", ""))
                            if audio:
                                await ws_wrap.send(json.dumps({"action": "startSpeaking"}))
                                await ws_wrap.send(audio)
                                await ws_wrap.send(json.dumps({"action": "stopSpeaking"}))
                                
                    elif msg.type in (aiohttp.WSMsgType.CLOSED, aiohttp.WSMsgType.ERROR):
                        break

                except asyncio.TimeoutError:
                    # ─── Silence detected ─────────────────────────────────────
                    if len(audio_buffer) >= MIN_AUDIO_BYTES:
                        await self._process_audio_buffer(ws_wrap, audio_buffer)
                        audio_buffer.clear()
                        await ws_wrap.send(json.dumps({"action": "listening"}))
                    elif audio_buffer:
                        logger.info(
                            f"Buffer too short ({len(audio_buffer)} bytes < "
                            f"{MIN_AUDIO_BYTES}), discarding"
                        )
                        audio_buffer.clear()
                    # If buffer empty and silence — stay in listening, do nothing

        except Exception as e:
            logger.error(f"Handler error: {e}", exc_info=True)
        finally:
            logger.info("Client disconnected")
            return websocket


async def health_check(request):
    from aiohttp import web
    return web.Response(text="OK")

async def main():
    server = AIServer()
    await server.init_asr()
    await server.init_llm()
    await server.init_tts()

    from aiohttp import web
    app = web.Application()
    app.router.add_get('/', server.handle_message)
    app.router.add_get('/ws', server.handle_message)
    app.router.add_get('/healthz', health_check)
    app.router.add_head('/', health_check)
    app.router.add_head('/healthz', health_check)

    port = int(os.environ.get("PORT", PORT))
    
    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, '0.0.0.0', port)
    await site.start()
    
    logger.info(f"P8 Server listening on 0.0.0.0:{port} using aiohttp")
    logger.info(f"  ASR: {DEFAULT_ASR}")
    logger.info(f"  LLM: {DEFAULT_LLM}")
    logger.info(f"  TTS: {DEFAULT_TTS}")
    
    await asyncio.Future()  # Run forever


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("Server stopped")