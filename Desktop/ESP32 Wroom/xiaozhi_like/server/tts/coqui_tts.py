# TTS - Coqui TTS Provider (local)
import asyncio
import logging
import tempfile
import os
from typing import Optional
from ..config import TTSConfig

logger = logging.getLogger(__name__)


class CoquiTTS:
    """Coqui TTS (local, requires model download)"""
    
    def __init__(self, config: TTSConfig):
        self.config = config
        self.model = config.COQUI_MODEL
        self.tts = None
        self._loaded = False
    
    async def load(self):
        """Load Coqui TTS model"""
        if self._loaded:
            return
        
        try:
            from TTS.api import TTS
            
            logger.info(f"Loading Coqui TTS model: {self.model}")
            self.tts = TTS(model_name=self.model, gpu=False)
            self._loaded = True
            logger.info("Coqui TTS loaded")
            
        except Exception as e:
            logger.error(f"Failed to load Coqui TTS: {e}")
            raise
    
    async def synthesize(self, text: str) -> Optional[bytes]:
        """
        Synthesize speech
        
        Args:
            text: Text to convert
        
        Returns:
            WAV audio data or None
        """
        if not self._loaded:
            await self.load()
        
        try:
            with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as f:
                temp_path = f.name
            
            try:
                # Generate WAV
                self.tts.tts_to_file(text=text, file_path=temp_path)
                
                with open(temp_path, 'rb') as f:
                    audio_data = f.read()
                
                logger.info(f"Coqui TTS: {len(text)} chars -> {len(audio_data)} bytes")
                return audio_data
                
            finally:
                if os.path.exists(temp_path):
                    os.unlink(temp_path)
                    
        except Exception as e:
            logger.error(f"Coqui TTS error: {e}")
            return None
    
    async def synthesize_stream(self, text: str):
        """Stream audio chunks"""
        if not self._loaded:
            await self.load()
        
        try:
            # Generate to file first (simpler)
            audio_data = await self.synthesize(text)
            if audio_data:
                yield audio_data
                
        except Exception as e:
            logger.error(f"Coqui TTS stream error: {e}")