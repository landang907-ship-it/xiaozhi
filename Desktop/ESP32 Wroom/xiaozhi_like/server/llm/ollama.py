# LLM - Ollama Provider (local, free)
import asyncio
import logging
from typing import Optional, AsyncIterator
import aiohttp
from ..config import LLMConfig

logger = logging.getLogger(__name__)


class OllamaLLM:
    """Ollama local LLM (no API key needed)"""
    
    def __init__(self, config: LLMConfig):
        self.config = config
        self.base_url = config.OLLAMA_BASE_URL
        self.model = config.OLLAMA_MODEL
        self.timeout = config.OLLAMA_TIMEOUT
    
    async def generate(self, prompt: str, system_prompt: str = "") -> Optional[str]:
        """
        Generate response from LLM
        
        Args:
            prompt: User prompt
            system_prompt: System instructions
        
        Returns:
            Generated text or None on error
        """
        try:
            url = f"{self.base_url}/api/generate"
            
            payload = {
                "model": self.model,
                "prompt": prompt,
                "stream": False,
                "options": {
                    "temperature": 0.7,
                    "top_p": 0.9,
                }
            }
            
            if system_prompt:
                payload["system"] = system_prompt
            
            async with aiohttp.ClientSession() as session:
                async with session.post(
                    url, 
                    json=payload,
                    timeout=aiohttp.ClientTimeout(total=self.timeout)
                ) as resp:
                    if resp.status == 200:
                        data = await resp.json()
                        text = data.get("response", "")
                        logger.info(f"Ollama response: {text[:50]}...")
                        return text.strip()
                    else:
                        error = await resp.text()
                        logger.error(f"Ollama error: {resp.status} - {error}")
                        return None
                        
        except asyncio.TimeoutError:
            logger.error("Ollama timeout")
            return None
        except Exception as e:
            logger.error(f"Ollama generation error: {e}")
            return None
    
    async def generate_stream(self, prompt: str, system_prompt: str = "") -> AsyncIterator[str]:
        """
        Stream response from LLM
        
        Yields:
            Text chunks as they arrive
        """
        try:
            url = f"{self.base_url}/api/generate"
            
            payload = {
                "model": self.model,
                "prompt": prompt,
                "stream": True,
                "options": {
                    "temperature": 0.7,
                    "top_p": 0.9,
                }
            }
            
            if system_prompt:
                payload["system"] = system_prompt
            
            async with aiohttp.ClientSession() as session:
                async with session.post(
                    url,
                    json=payload,
                    timeout=aiohttp.ClientTimeout(total=self.timeout)
                ) as resp:
                    if resp.status == 200:
                        async for line in resp.content:
                            if line:
                                try:
                                    data = line.decode().strip()
                                    if data.startswith("data:"):
                                        data = data[5:]
                                    if data:
                                        import json
                                        chunk = json.loads(data)
                                        if "response" in chunk:
                                            yield chunk["response"]
                                except:
                                    pass
                    else:
                        logger.error(f"Ollama stream error: {resp.status}")
                        
        except Exception as e:
            logger.error(f"Ollama stream error: {e}")
    
    async def check_connection(self) -> bool:
        """Check if Ollama server is running"""
        try:
            async with aiohttp.ClientSession() as session:
                async with session.get(f"{self.base_url}/api/tags") as resp:
                    return resp.status == 200
        except:
            return False