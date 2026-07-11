# ASR - Whisper Provider (OpenAI)
import asyncio
import logging
from typing import Optional
import aiohttp
from ..config import ASRConfig

logger = logging.getLogger(__name__)


class WhisperASR:
    """OpenAI Whisper API ASR"""
    
    def __init__(self, config: ASRConfig):
        self.config = config
        self.api_key = config.WHISPER_API_KEY
        self.model = config.WHISPER_MODEL
        self.url = config.WHISPER_URL
    
    async def transcribe(self, audio_data: bytes) -> Optional[str]:
        """
        Transcribe audio bytes to text
        
        Args:
            audio_data: Raw PCM audio (16kHz, 16-bit mono)
        
        Returns:
            Transcribed text or None on error
        """
        if not self.api_key:
            logger.warning("Whisper API key not set")
            return None
        
        try:
            # Convert PCM to WAV in memory
            import io
            import wave
            
            wav_buffer = io.BytesIO()
            with wave.open(wav_buffer, 'wb') as wf:
                wf.setnchannels(1)
                wf.setsampwidth(2)  # 16-bit
                wf.setframerate(16000)
                wf.writeframes(audio_data)
            
            wav_buffer.seek(0)
            wav_data = wav_buffer.read()
            
            # Send to Whisper API
            form = aiohttp.FormData()
            form.add_field('model', self.model)
            form.add_field('language', 'vi')  # Vietnamese
            form.add_field('response_format', 'text')
            form.add_field('file', wav_data, filename='audio.wav', content_type='audio/wav')
            
            headers = {'Authorization': f'Bearer {self.api_key}'}
            
            async with aiohttp.ClientSession() as session:
                async with session.post(self.url, data=form, headers=headers, timeout=aiohttp.ClientTimeout(total=30)) as resp:
                    if resp.status == 200:
                        text = await resp.text()
                        logger.info(f"Whisper transcription: {text[:50]}...")
                        return text.strip()
                    else:
                        error = await resp.text()
                        logger.error(f"Whisper API error: {resp.status} - {error}")
                        return None
                        
        except asyncio.TimeoutError:
            logger.error("Whisper API timeout")
            return None
        except Exception as e:
            logger.error(f"Whisper transcription error: {e}")
            return None
    
    async def transcribe_stream(self, audio_chunks: list) -> Optional[str]:
        """
        Transcribe from multiple audio chunks
        
        Args:
            audio_chunks: List of raw PCM audio bytes
        
        Returns:
            Transcribed text
        """
        # Combine all chunks
        combined = b''.join(audio_chunks)
        return await self.transcribe(combined)