# LLM - OpenAI Provider
import asyncio
import logging
from typing import Optional, AsyncIterator
from ..config import LLMConfig

logger = logging.getLogger(__name__)


class OpenAILLM:
    """OpenAI GPT API"""
    
    def __init__(self, config: LLMConfig):
        self.config = config
        self.api_key = config.OPENAI_API_KEY
        self.model = config.OPENAI_MODEL
    
    async def generate(self, prompt: str, system_prompt: str = "") -> Optional[str]:
        """Generate response from OpenAI GPT"""
        try:
            from openai import AsyncOpenAI
            
            client = AsyncOpenAI(api_key=self.api_key)
            
            messages = []
            if system_prompt:
                messages.append({"role": "system", "content": system_prompt})
            messages.append({"role": "user", "content": prompt})
            
            response = await client.chat.completions.create(
                model=self.model,
                messages=messages,
                temperature=0.7,
                max_tokens=500
            )
            
            text = response.choices[0].message.content
            logger.info(f"OpenAI response: {text[:50]}...")
            return text.strip()
            
        except Exception as e:
            logger.error(f"OpenAI error: {e}")
            return None
    
    async def generate_stream(self, prompt: str, system_prompt: str = "") -> AsyncIterator[str]:
        """Stream response from OpenAI"""
        try:
            from openai import AsyncOpenAI
            
            client = AsyncOpenAI(api_key=self.api_key)
            
            messages = []
            if system_prompt:
                messages.append({"role": "system", "content": system_prompt})
            messages.append({"role": "user", "content": prompt})
            
            stream = await client.chat.completions.create(
                model=self.model,
                messages=messages,
                stream=True,
                temperature=0.7,
                max_tokens=500
            )
            
            async for chunk in stream:
                if chunk.choices[0].delta.content:
                    yield chunk.choices[0].delta.content
                    
        except Exception as e:
            logger.error(f"OpenAI stream error: {e}")