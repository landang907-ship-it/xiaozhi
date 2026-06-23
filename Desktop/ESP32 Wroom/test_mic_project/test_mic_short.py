"""Capture only 5s of output (one boot cycle)."""
import serial, time, sys

port = sys.argv[1] if len(sys.argv) > 1 else 'COM6'
outfile = sys.argv[2] if len(sys.argv) > 2 else r'C:\Users\HP\Desktop\ESP32 Wroom\mic_short.log'

s = serial.Serial(port, 115200, timeout=2)
time.sleep(0.3)
try:
    s.reset_input_buffer()
except Exception:
    pass

end = time.time() + 5
with open(outfile, 'wb') as f:
    while time.time() < end:
        try:
            chunk = s.read(512)
            if chunk:
                f.write(chunk)
                f.flush()
        except Exception as e:
            f.write(f"\n[err {e}]\n".encode())
            break

s.close()

# Print file content
with open(outfile, 'rb') as f:
    data = f.read()
print(data.decode('utf-8', 'replace'))
