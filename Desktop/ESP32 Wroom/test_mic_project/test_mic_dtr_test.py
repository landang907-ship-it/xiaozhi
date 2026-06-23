"""test_mic_dtr_test.py — robust COM6 mic test with DTR reset.

Opens COM6, toggles DTR to reset ESP32, drains boot, then issues:
  init    -> I2S RX ready
  read N  -> peak/rms
  fft N   -> dominant frequency

Usage:
  python test_mic_dtr_test.py [PORT]
"""
import serial, time, sys, io

# Force UTF-8 stdout (Windows cp1252 otherwise breaks on non-breaking hyphens)
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

PORT = sys.argv[1] if len(sys.argv) > 1 else 'COM6'
BAUD = 115200

log = lambda m: print(m, flush=True)


def open_port():
    log(f"Open {PORT} @ {BAUD} ...")
    s = serial.Serial(PORT, BAUD, timeout=1)
    s.dtr = False
    s.rts = False
    time.sleep(0.2)
    s.dtr = True
    s.rts = True
    time.sleep(0.2)
    s.dtr = False
    s.rts = False
    time.sleep(0.5)
    try:
        s.reset_input_buffer()
    except Exception:
        pass
    return s


def drain(s, seconds):
    end = time.time() + seconds
    while time.time() < end:
        s.read(512)


def send_and_read(s, cmd, wait, max_bytes):
    s.write(cmd.encode() + b"\n")
    time.sleep(wait)
    return s.read(max_bytes)


def main():
    s = open_port()
    log("Drain boot 3s ...")
    drain(s, 3)
    try:
        s.reset_input_buffer()
    except Exception:
        pass

    for cmd, wait, n in [("init", 2, 2048),
                          ("read 1024", 3, 8192),
                          ("fft 1024", 3, 16384),
                          ("read 2048", 3, 16384),
                          ("fft 2048", 3, 32768)]:
        log(f"--- {cmd} ---")
        resp = send_and_read(s, cmd, wait, n)
        log(f"({len(resp)} bytes) {resp.decode('utf-8', 'replace').strip()[:1500]}")

    s.close()
    log("=== DONE ===")


if __name__ == "__main__":
    main()
