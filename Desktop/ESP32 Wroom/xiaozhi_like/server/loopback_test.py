#!/usr/bin/env python3
"""
Audio Loopback Test Server - P8
Test mic -> ESP32 -> WebSocket -> Server -> Speaker

Server echoes audio back to ESP32 for real-time loopback test.
"""
import asyncio
import json
import logging
import struct
import wave
import io
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import websockets

logging.basicConfig(level=logging.INFO, format='%(asctime)s [LOOPBACK] %(message)s')
logger = logging.getLogger("loopback")

HOST = "0.0.0.0"
PORT = 8080

# Stats
packets_received = 0
total_bytes_received = 0
total_bytes_sent = 0
clients = 0


async def loopback_handler(websocket, path=None):
    global packets_received, total_bytes_received, total_bytes_sent, clients
    clients += 1
    client_id = clients
    logger.info(f"[Client #{client_id}] Connected from {websocket.remote_address}")

    try:
        async for message in websocket:
            if isinstance(message, bytes):
                # Audio data received from ESP32
                # Echo it back immediately (loopback test)
                packets_received += 1
                total_bytes_received += len(message)
                await websocket.send(message)
                total_bytes_sent += len(message)
                
                if packets_received % 50 == 0:
                    logger.info(
                        f"[Client #{client_id}] Packets: {packets_received}, "
                        f"RX: {total_bytes_received}B, TX: {total_bytes_sent}B"
                    )

            elif isinstance(message, str):
                # JSON commands
                try:
                    data = json.loads(message)
                    cmd = data.get("cmd", "")
                    
                    if cmd == "ping":
                        await websocket.send(json.dumps({"type": "pong"}))
                        logger.info(f"[Client #{client_id}] Ping-pong")
                    
                    elif cmd == "echo_start":
                        logger.info(f"[Client #{client_id}] Echo mode STARTED")
                        await websocket.send(json.dumps({"type": "echo_ready"}))
                    
                    elif cmd == "echo_stop":
                        logger.info(f"[Client #{client_id}] Echo mode STOPPED")
                        await websocket.send(json.dumps({"type": "echo_stopped"}))
                    
                    elif cmd == "status":
                        await websocket.send(json.dumps({
                            "type": "status",
                            "packets_received": packets_received,
                            "total_bytes_received": total_bytes_received,
                            "total_bytes_sent": total_bytes_sent,
                            "clients": clients
                        }))
                    
                    else:
                        logger.warning(f"[Client #{client_id}] Unknown cmd: {cmd}")

                except json.JSONDecodeError:
                    logger.warning(f"[Client #{client_id}] Invalid JSON: {message[:100]}")

    except websockets.exceptions.ConnectionClosed:
        logger.info(f"[Client #{client_id}] Disconnected")
    except Exception as e:
        logger.error(f"[Client #{client_id}] Error: {e}")
    finally:
        clients -= 1
        logger.info(f"[Client #{client_id}] Cleaned up. Active clients: {clients}")


async def main():
    logger.info(f"Starting Loopback Test Server on ws://{HOST}:{PORT}")
    logger.info("Commands:")
    logger.info("  ESP32: ws ws://<PC_IP>:8766")
    logger.info("  ESP32: echo_start / echo_stop / status / ping")
    logger.info("Press Ctrl+C to stop")
    
    async with websockets.serve(loopback_handler, HOST, PORT):
        await asyncio.Future()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("Server stopped by user")