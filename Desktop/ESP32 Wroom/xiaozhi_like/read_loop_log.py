"""Monitor xiaozhi_like serial output for 15 seconds, with longer initial delay."""

import serial, time, sys, threading

port = sys.argv[1] if len(sys.argv) > 1 else 'COM6'
s = serial.Serial(port, 115200, timeout=15)
print(f"[monitor] Opened {port}, waiting 3 sec for device enumeration...")
time.sleep(3)
s.reset_input_buffer()

buf = []
def reader():
    while True:
        try:
            chunk = s.read(256)
            if chunk:
                text = chunk.decode('utf-8', 'replace')
                buf.append(text)
                sys.stdout.write(text)
                sys.stdout.flush()
        except:
            break

t = threading.Thread(target=reader, daemon=True)
t.start()

print(f"[monitor] Listening on {port} for 15 seconds...")
time.sleep(15)
print("\n[monitor] Done.")
s.close()
