"""Read COM6 for ~15 seconds, send init/read/fft commands, dump to log."""
import serial, time, sys, threading

LOG = "C:/Users/HP/Desktop/ESP32 Wroom/test_mic_project/mic_test.log"

port = sys.argv[1] if len(sys.argv) > 1 else 'COM6'
s = serial.Serial(port, 115200, timeout=1)
time.sleep(0.5)
try:
    s.reset_input_buffer()
except Exception:
    pass

logf = open(LOG, "wb", 0)

stop = [False]
def reader():
    while not stop[0]:
        try:
            c = s.read(256)
            if c:
                sys.stdout.write(c.decode('utf-8', 'replace'))
                sys.stdout.flush()
                logf.write(c)
        except Exception:
            break

t = threading.Thread(target=reader, daemon=True)
t.start()

s.write(b"\x03")
time.sleep(0.3)
time.sleep(2.5)
s.write(b"init\n")
time.sleep(3)
s.write(b"read 1024\n")
time.sleep(3)
s.write(b"fft 1024\n")
time.sleep(3)

stop[0] = True
time.sleep(0.3)
s.close()
logf.close()
print("\n=== DONE ===")
