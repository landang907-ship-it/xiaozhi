"""Capture full serial output for test_mic, save to file."""
import serial, time, sys

port = sys.argv[1] if len(sys.argv) > 1 else 'COM6'
outfile = sys.argv[2] if len(sys.argv) > 2 else r'C:\Users\HP\Desktop\ESP32 Wroom\mic_capture.log'

s = serial.Serial(port, 115200, timeout=2)
time.sleep(0.5)
try:
    s.reset_input_buffer()
except Exception:
    pass

end = time.time() + 20
lines = []
with open(outfile, 'w', encoding='utf-8') as f:
    while time.time() < end:
        try:
            chunk = s.read(512)
            if chunk:
                txt = chunk.decode('utf-8', 'replace')
                f.write(txt)
                f.flush()
                lines.append(txt)
        except Exception as e:
            f.write(f"\n[err {e}]\n")
            break

s.close()
print(f"=== Captured {sum(len(x) for x in lines)} chars to {outfile} ===")
print(''.join(lines))
