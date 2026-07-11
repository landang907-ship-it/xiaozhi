# TTS - SillyTavern Provider
import asyncio
import logging
from typing import Optional
import aiohttp
from ..config import TTSConfig

logger = logging.getLogger(__name__)


class SillyTavernTTS:
    """SillyTavern TTS API (requires existing SillyTavern installation)"""
    
    def __init__(self, config: TTSConfig):
        self.config = config
        self.url = config.SILLYTAVERN_URL.rstrip('/')
        self.api_key = config.SILLYTAVERN_API_KEY
    
    async def synthesize(self, text: str, voice_id: str = "default") -> Optional[bytes]:
        """
        Synthesize speech via SillyTavern
        
        Args:
            text: Text to convert
            voice_id: Voice identifier
        
        Returns:
            Audio data or None
        """
        try:
            url = f"{self.url}/api/tts"
            
            headers = {}
            if self.api_key:
                headers["Authorization"] = f"Bearer {self.api_key}"
            
            payload = {
                "text": text,
                "voice": voice_id,
            }
            
            async with aiohttp.ClientSession() as session:
                async with session.post(
                    url,
                    json=payload,
                    headers=headers,
                    timeout=aiohttp.ClientTimeout(total=30)
                ) as resp:
                    if resp.status == 200:
                        audio_data = await resp.read()
                        logger.info(f"SillyTavern TTS: {len(audio_data)} bytes")
                        return audio_data
                    else:
                        error = await resp.text()
                        logger.error(f"SillyTavern error: {resp.status} - {error}")
                        return None
                        
        except Exception as e:
            logger.error(f"SillyTavern TTS error: {e}")
            return None
    
    async def get_voices(self) -> list:
        """Get available voices"""
        try:
            url = f"{self.url}/api/voices"
            
            headers = {}
            if self.api_key:
                headers["Authorization"] = f"Bearer {self.api_key}"
            
            async with aiohttp.ClientSession() as session:
                async with session.get(url, headers=headers) as resp:
                    if resp.status == 200:
                        return await resp.json()
                    return []
                    
        except Exception as e:
            logger.error(f"Get voices error: {e}")
            return []