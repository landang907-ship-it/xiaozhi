"""Mở COM6, đợi boot, gửi 'a', đọc 90s, ghi raw ra file."""
import serial, time, sys, threading

PORT = "COM6"
BAUD = 115200
OUT = r"C:\Users\HP\Desktop\ESP32 Wroom\test_oled_project\out_a.txt"
ERROUT = r"C:\Users\HP\Desktop\ESP32 Wroom\test_oled_project\out_a.err.txt"

def main():
    # Reset ESP32 bằng cách toggle DTR (cho auto-reset circuit)
    s0 = serial.Serial(PORT, 1200, timeout=0.1)
    s0.dtr = False; s0.rts = False
    time.sleep(0.1)
    s0.dtr = True;  s0.rts = True
    time.sleep(0.1)
    s0.dtr = False; s0.rts = False
    s0.close()
    time.sleep(0.2)

    s = serial.Serial(PORT, BAUD, timeout=0.1)
    # Bỏ boot log khoảng 3s
    t_end = time.time() + 3
    while time.time() < t_end:
        s.read(4096)

    # Reset buffer, gửi 'a'
    s.reset_input_buffer()
    s.write(b"a\n")
    s.flush()

    f = open(OUT, "wb")
    f.write(b">>> sent 'a' <<<\n")
    t_end = time.time() + 90
    while time.time() < t_end:
        data = s.read(4096)
        if data:
            f.write(data); f.flush()
    f.close()
    s.close()
    print("done", flush=True)

if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        with open(ERROUT, "w") as f:
            f.write(repr(e) + "\n")
        raise
