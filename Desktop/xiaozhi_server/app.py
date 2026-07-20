import asyncio
import json
import logging
import av
import wave
import tempfile
import os
from websockets.server import serve

from ai_handler import generate_response
from tts_handler import generate_tts

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("Xiaozhi-Server")

async def encode_mp3_to_opus_packets(mp3_path):
    """
    Reads an MP3 file (from edge-tts) and yields raw Opus packets (16kHz, mono, 60ms).
    """
    packets = []
    try:
        # Open the MP3 file
        container = av.open(mp3_path)
        audio_stream = container.streams.audio[0]
        
        # Create an Opus encoder
        opus_encoder = av.CodecContext.create("opus", "w")
        opus_encoder.sample_rate = 16000
        opus_encoder.channels = 1
        opus_encoder.format = 's16'
        
        # Resampler if needed (MP3 from edge-tts is usually 24kHz or 48kHz)
        resampler = av.AudioResampler(format='s16', layout='mono', rate=16000)
        
        # 60ms frames at 16000Hz = 960 samples per frame
        # We need to accumulate samples before encoding
        accumulated_samples = bytearray()
        
        for frame in container.decode(audio_stream):
            frame.pts = None
            resampled_frames = resampler.resample(frame)
            for r_frame in resampled_frames:
                accumulated_samples.extend(r_frame.to_ndarray().tobytes())
                
                # 960 samples * 2 bytes/sample (s16) = 1920 bytes
                while len(accumulated_samples) >= 1920:
                    chunk = accumulated_samples[:1920]
                    accumulated_samples = accumulated_samples[1920:]
                    
                    # Create an AudioFrame from the raw bytes
                    import numpy as np
                    raw_array = np.frombuffer(chunk, dtype=np.int16).reshape(1, -1)
                    audio_frame = av.AudioFrame.from_ndarray(raw_array, format='s16', layout='mono')
                    audio_frame.sample_rate = 16000
                    
                    enc_packets = opus_encoder.encode(audio_frame)
                    for p in enc_packets:
                        packets.append(bytes(p))
                        
        # Flush encoder
        for p in opus_encoder.encode():
            packets.append(bytes(p))
            
    except Exception as e:
        logger.error(f"Error encoding audio: {e}")
        
    return packets

async def handler(websocket):
    client_id = id(websocket)
    logger.info(f"Client connected: {client_id}")
    
    # Session state
    is_listening = False
    opus_decoder = av.CodecContext.create('opus', 'r')
    opus_decoder.sample_rate = 16000
    opus_decoder.channels = 1
    
    pcm_buffer = bytearray()
    
    try:
        async for message in websocket:
            if isinstance(message, str):
                try:
                    data = json.loads(message)
                    msg_type = data.get("type")
                    
                    if msg_type == "hello":
                        logger.info("Received hello from ESP32")
                        await websocket.send(json.dumps({"type": "hello"}))
                        
                    elif msg_type == "listen":
                        state = data.get("state")
                        if state == "start":
                            logger.info("Started listening (VAD start)")
                            is_listening = True
                            pcm_buffer = bytearray()
                            opus_decoder = av.CodecContext.create('opus', 'r')
                            opus_decoder.sample_rate = 16000
                            opus_decoder.channels = 1
                        elif state == "stop":
                            logger.info("Stopped listening (VAD stop)")
                            is_listening = False
                            
                            if len(pcm_buffer) > 0:
                                # Save PCM to WAV
                                fd, wav_path = tempfile.mkstemp(suffix=".wav")
                                with wave.open(os.fdopen(fd, 'wb'), 'wb') as wf:
                                    wf.setnchannels(1)
                                    wf.setsampwidth(2)
                                    wf.setframerate(16000)
                                    wf.writeframes(pcm_buffer)
                                
                                # Send to Gemini
                                await websocket.send(json.dumps({"type": "stt", "text": "Đang suy nghĩ..."}))
                                logger.info("Sending to Gemini...")
                                
                                text_resp, func_calls = await generate_response(wav_path)
                                os.remove(wav_path)
                                
                                # Send LLM response text
                                await websocket.send(json.dumps({"type": "llm", "text": text_resp}))
                                
                                # Handle Function Calls (MCP)
                                for fc in func_calls:
                                    logger.info(f"Function Call: {fc}")
                                    await websocket.send(json.dumps({
                                        "type": "mcp",
                                        "name": fc["name"],
                                        "arguments": fc["arguments"]
                                    }))
                                
                                # Generate TTS
                                await websocket.send(json.dumps({"type": "tts", "state": "start"}))
                                
                                mp3_path = await generate_tts(text_resp)
                                opus_packets = await encode_mp3_to_opus_packets(mp3_path)
                                os.remove(mp3_path)
                                
                                # Send audio packets
                                for packet in opus_packets:
                                    await websocket.send(packet)
                                    await asyncio.sleep(0.02) # simulate 20ms streaming
                                
                                await websocket.send(json.dumps({"type": "tts", "state": "stop"}))
                                
                except json.JSONDecodeError:
                    pass
            elif isinstance(message, bytes):
                if is_listening:
                    try:
                        packet = av.Packet(message)
                        frames = opus_decoder.decode(packet)
                        for frame in frames:
                            pcm_buffer.extend(frame.to_ndarray().tobytes())
                    except Exception as e:
                        logger.error(f"Opus decode error: {e}")
                        
    except Exception as e:
        logger.error(f"Connection error: {e}")
    finally:
        logger.info(f"Client disconnected: {client_id}")

async def main():
    async with serve(handler, "0.0.0.0", 8080):
        logger.info("Xiaozhi Python Server started on ws://0.0.0.0:8080")
        await asyncio.Future()  # run forever

if __name__ == "__main__":
    asyncio.run(main())
