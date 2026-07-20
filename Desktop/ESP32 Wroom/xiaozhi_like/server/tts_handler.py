import asyncio
import edge_tts
import tempfile
import os

async def generate_tts(text: str, voice: str = "vi-VN-HoaiMyNeural") -> str:
    """
    Generates TTS audio and returns the path to the temporary wav file.
    """
    communicate = edge_tts.Communicate(text, voice)
    
    # Create a temporary file
    fd, path = tempfile.mkstemp(suffix=".mp3")
    os.close(fd)
    
    # Save the TTS audio to the file
    await communicate.save(path)
    return path
