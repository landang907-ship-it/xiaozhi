# LLM - Anthropic Claude Provider
import asyncio
import logging
from typing import Optional, AsyncIterator
from ..config import LLMConfig

logger = logging.getLogger(__name__)


class AnthropicLLM:
    """Anthropic Claude API"""
    
    def __init__(self, config: LLMConfig):
        self.config = config
        self.api_key = config.ANTHROPIC_API_KEY
        self.model = config.ANTHROPIC_MODEL
    
    async def generate(self, prompt: str, system_prompt: str = "") -> Optional[str]:
        """Generate response from Claude"""
        try:
            import anthropic
            
            client = anthropic.AsyncAnthropic(api_key=self.api_key)
            
            messages = [{"role": "user", "content": prompt}]
            
            response = await client.messages.create(
                model=self.model,
                system=system_prompt,
                messages=messages,
                max_tokens=500
            )
            
            text = response.content[0].text
            logger.info(f"Claude response: {text[:50]}...")
            return text.strip()
            
        except Exception as e:
            logger.error(f"Claude error: {e}")
            return None