FROM python:3.11-slim

# Install system dependencies required for miniaudio and edge-tts
RUN apt-get update && apt-get install -y \
    gcc \
    ffmpeg \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Install Python dependencies
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy server code
COPY . .

# Expose port (Render/HF Spaces will override this, but good practice)
EXPOSE 8080

# Run the server
CMD ["python", "main.py"]
