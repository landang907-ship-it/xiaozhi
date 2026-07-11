# TTS - Edge TTS Provider (free, cloud)
import asyncio
import logging
import tempfile
import os
from typing import Optional
from ..config import TTSConfig

logger = logging.getLogger(__name__)


class EdgeTTS:
    """Microsoft Edge TTS (free, no API key)"""
    
    def __init__(self, config: TTSConfig):
        self.config = config
        self.voice = config.EDGE_VOICE
        self.rate = config.EDGE_RATE
        self.pitch = config.EDGE_PITCH
    
    async def synthesize(self, text: str) -> Optional[bytes]:
        """
        Synthesize speech from text
        
        Args:
            text: Text to convert to speech
        
        Returns:
            Audio data (MP3 format) or None on error
        """
        try:
            import edge_tts
            
            # Generate to temp file then read
            with tempfile.NamedTemporaryFile(suffix='.mp3', delete=False) as f:
                temp_path = f.name
            
            try:
                communicate = edge_tts.Communicate(
                    text,
                    voice=self.voice,
                    rate=self.rate,
                    pitch=self.pitch
                )
                await communicate.save(temp_path)
                
                with open(temp_path, 'rb') as f:
                    audio_data = f.read()
                
                logger.info(f"Edge TTS: synthesized {len(text)} chars -> {len(audio_data)} bytes")
                return audio_data
                
            finally:
                if os.path.exists(temp_path):
                    os.unlink(temp_path)
                    
        except Exception as e:
            logger.error(f"Edge TTS error: {e}")
            return None
    
    async def synthesize_to_file(self, text: str, output_path: str) -> bool:
        """Synthesize and save to file"""
        try:
            import edge_tts
            
            communicate = edge_tts.Communicate(
                text,
                voice=self.voice,
                rate=self.rate,
                pitch=self.pitch
            )
            await communicate.save(output_path)
            return True
            
        except Exception as e:
            logger.error(f"Edge TTS error: {e}")
            return False
    
    async def synthesize_stream(self, text: str):
        """
        Stream audio data
        
        Yields:
            Audio chunks as they are generated
        """
        try:
            import edge_tts
            
            async for chunk in edge_tts.Communicate(
                text,
                voice=self.voice,
                rate=self.rate,
                pitch=self.pitch
            ).stream():
                if chunk["type"] == "audio":
                    yield chunk["data"]
                    
        except Exception as e:
            logger.error(f"Edge TTS stream error: {e}")