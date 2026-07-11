# ASR - Silero VAD (Voice Activity Detection) + Silero STT
import asyncio
import logging
from typing import Optional, List
import numpy as np

logger = logging.getLogger(__name__)


class SileroASR:
    """Silero VAD + STT (free, local)"""
    
    def __init__(self, config):
        self.config = config
        self.model = None
        self.utils = None
        self._loaded = False
    
    async def load(self):
        """Load Silero models (downloads on first run)"""
        if self._loaded:
            return
        
        try:
            import torch
            from pathlib import Path
            
            logger.info("Loading Silero VAD model...")
            
            # Silero VAD model
            torch.set_num_threads(1)
            self.model, utils = torch.hub.load(
                repo_or_dir='snakers4/silero-vad',
                model='silero_vad',
                trust_repo=True
            )
            self.get_speech_timestamps, self.read_audio, self VADIterator, _, _ = utils
            
            self._loaded = True
            logger.info("Silero VAD loaded successfully")
            
        except Exception as e:
            logger.error(f"Failed to load Silero: {e}")
            raise
    
    async def detect_speech(self, audio_data: bytes, sample_rate: int = 16000) -> List[dict]:
        """
        Detect speech segments in audio
        
        Returns:
            List of dicts with 'start' and 'end' indices
        """
        if not self._loaded:
            await self.load()
        
        try:
            import torch
            
            # Convert bytes to numpy array
            audio_np = np.frombuffer(audio_data, dtype=np.int16).astype(np.float32) / 32768.0
            
            # Get speech timestamps
            speech_timestamps = self.get_speech_timestamps(
                audio_np,
                self.model,
                sampling_rate=sample_rate,
                threshold=self.config.SILERO_THRESHOLD
            )
            
            return speech_timestamps
            
        except Exception as e:
            logger.error(f"VAD error: {e}")
            return []
    
    async def transcribe(self, audio_data: bytes) -> Optional[str]:
        """
        Silero STT - requires separate model for actual transcription
        For now, returns detected speech segments as timing info
        """
        if not self._loaded:
            await self.load()
        
        segments = await self.detect_speech(audio_data)
        
        if segments:
            # Return info about speech segments
            total_speech_ms = sum(s['end'] - s['start'] for s in segments)
            logger.info(f"Detected {len(segments)} speech segments, {total_speech_ms}ms total")
            return f"[speech detected: {total_speech_ms}ms in {len(segments)} segments]"
        
        return None