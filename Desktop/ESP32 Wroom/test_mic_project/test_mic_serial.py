"""Send commands to test_mic REPL and capture output."""
import serial, time, sys, threading

port = sys.argv[1] if len(sys.argv) > 1 else 'COM6'
s = serial.Serial(port, 115200, timeout=1)
time.sleep(0.3)
try:
    s.reset_input_buffer()
except Exception:
    pass

stop = False
def reader():
    global stop
    buf = bytearray()
    while not stop:
        try:
            chunk = s.read(256)
            if chunk:
                buf.extend(chunk)
                sys.stdout.write(chunk.decode('utf-8', 'replace'))
                sys.stdout.flush()
        except Exception:
            break

t = threading.Thread(target=reader, daemon=True)
t.start()

# Wait a bit for boot
time.sleep(3)
# Send init
s.write(b"init\n")
time.sleep(2)
# Send read 1024
s.write(b"read 1024\n")
time.sleep(3)
# Send read 1024 again (in case of capture)
s.write(b"read 1024\n")
time.sleep(3)
# Try fft
s.write(b"fft 1024\n")
time.sleep(3)
# Try watch
s.write(b"watch 5\n")
time.sleep(7)

stop = True
time.sleep(0.5)
s.close()
print("\n=== Done ===")
