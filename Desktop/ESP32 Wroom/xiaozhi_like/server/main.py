import asyncio
import json
import logging
import av
import wave
import tempfile
import os
import time
import numpy as np
from aiohttp import web

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("Xiaozhi-Server")

from ai_handler import generate_response
from tts_handler import generate_tts

# Minimum 0.5 seconds of 16kHz 16-bit mono PCM audio required to trigger AI (16000 * 2 * 0.5 = 16000 bytes)
MIN_AUDIO_BYTES = 16000

async def encode_mp3_to_opus_packets(mp3_path):
    """
    Reads an MP3 file (from edge-tts) and yields raw Opus packets (16kHz, mono, 60ms).
    """
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
                
                while len(accumulated_samples) >= 1920: # 960 samples * 2 bytes = 60ms
                    chunk = accumulated_samples[:1920]
                    accumulated_samples = accumulated_samples[1920:]
                    
                    raw_array = np.frombuffer(chunk, dtype=np.int16).reshape(1, -1)
                    audio_frame = av.AudioFrame.from_ndarray(raw_array, format='s16', layout='mono')
                    audio_frame.sample_rate = 16000
                    
                    enc_packets = opus_encoder.encode(audio_frame)
                    for p in enc_packets:
                        packets.append(bytes(p))
                        
        for p in opus_encoder.encode():
            packets.append(bytes(p))
            
    except Exception as e:
        logger.error(f"Error encoding audio: {e}")
        
    return packets

async def websocket_handler(request):
    websocket = web.WebSocketResponse(heartbeat=20.0)
    await websocket.prepare(request)
    
    client_id = id(websocket)
    logger.info(f"Client connected: {client_id}")
    
    is_listening = False
    opus_decoder = None
    decoder_resampler = None
    pcm_buffer = bytearray()
    last_audio_time = 0
    is_processing = False
    
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
        nonlocal is_listening, pcm_buffer, is_processing
        if is_processing or len(pcm_buffer) < MIN_AUDIO_BYTES:
            pcm_buffer.clear()
            return
        
        is_processing = True
        is_listening = False
        buffer_to_process = bytes(pcm_buffer)
        pcm_buffer.clear()
        
        logger.info(f"Processing recorded audio ({len(buffer_to_process)} bytes)...")
        
        try:
            # Save PCM to WAV (16kHz, 16-bit Mono)
            fd, wav_path = tempfile.mkstemp(suffix=".wav")
            with wave.open(os.fdopen(fd, 'wb'), 'wb') as wf:
                wf.setnchannels(1)
                wf.setsampwidth(2)
                wf.setframerate(16000)
                wf.writeframes(buffer_to_process)
            
            # Send STT thinking status
            await safe_send_str(json.dumps({"type": "stt", "text": "Đang suy nghĩ..."}))
            logger.info("Sending audio to Gemini AI...")
            
            text_resp, func_calls = await generate_response(wav_path)
            if os.path.exists(wav_path):
                os.remove(wav_path)
            
            logger.info(f"Gemini Response: {text_resp}")
            
            # Send LLM text response
            await safe_send_str(json.dumps({"type": "llm", "text": text_resp}))
            
            # Handle MCP Function Calls
            for fc in func_calls:
                logger.info(f"Function Call: {fc}")
                await safe_send_str(json.dumps({
                    "type": "mcp",
                    "name": fc["name"],
                    "arguments": fc["arguments"]
                }))
            
            # Generate TTS MP3 and encode to Opus packets BEFORE telling ESP32 to start TTS!
            mp3_path = await generate_tts(text_resp)
            opus_packets = await encode_mp3_to_opus_packets(mp3_path)
            if os.path.exists(mp3_path):
                os.remove(mp3_path)
            
            # Send TTS start and text sentence
            await safe_send_str(json.dumps({"type": "tts", "state": "sentence_start", "text": text_resp}))
            await safe_send_str(json.dumps({"type": "tts", "state": "start"}))
            
            logger.info(f"Streaming {len(opus_packets)} Opus packets to ESP32...")
            for i, packet in enumerate(opus_packets):
                if websocket.closed:
                    logger.warning("Websocket closed during audio streaming, aborting stream.")
                    break
                await safe_send_bytes(packet)
                if i % 3 == 2:
                    await asyncio.sleep(0.04) # 60ms audio every 40ms
            
            await safe_send_str(json.dumps({"type": "tts", "state": "stop"}))
            logger.info("Finished streaming response to ESP32")
            
        except Exception as e:
            logger.error(f"Error in process_audio: {e}")
        finally:
            is_processing = False

    async def silence_checker():
        """Silence detection after user stops speaking for 0.8 seconds (requires min 0.5s audio)"""
        nonlocal last_audio_time
        while True:
            await asyncio.sleep(0.2)
            if is_listening and not is_processing:
                if len(pcm_buffer) < MIN_AUDIO_BYTES:
                    # Ignore tiny noise (<0.5s)
                    if time.time() - last_audio_time > 1.5:
                        pcm_buffer.clear()
                else:
                    if time.time() - last_audio_time > 0.8:
                        logger.info("Silence detected (0.8s timeout). Triggering AI processing...")
                        asyncio.create_task(process_audio())
                    
    checker_task = asyncio.create_task(silence_checker())
    
    try:
        async for message in websocket:
            if message.type == web.WSMsgType.TEXT:
                try:
                    data = json.loads(message.data)
                    msg_type = data.get("type")
                    
                    if msg_type == "hello":
                        logger.info("Received hello from ESP32, replying with audio_params (16000Hz)")
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
                            logger.info("Started listening (VAD start)")
                            is_listening = True
                            pcm_buffer.clear()
                            opus_decoder = av.CodecContext.create('opus', 'r')
                            opus_decoder.sample_rate = 16000
                            opus_decoder.layout = 'mono'
                            decoder_resampler = av.AudioResampler(format='s16', layout='mono', rate=16000)
                            last_audio_time = time.time()
                        elif state == "stop":
                            logger.info("Stopped listening (VAD stop)")
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
        logger.info(f"Client disconnected: {client_id}")
    
    return websocket

app = web.Application()
app.router.add_get('/', websocket_handler)

if __name__ == "__main__":
    web.run_app(app, host='0.0.0.0', port=8080)
