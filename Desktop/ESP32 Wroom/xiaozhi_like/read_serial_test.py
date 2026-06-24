import serial, time, threading

data = []
stop = False

def reader():
    while not stop:
        try:
            d = s.read(200)
            if d:
                data.append(d)
                print(f"[RCV {len(d)} bytes]", end='', flush=True)
        except:
            pass

s = serial.Serial('COM6', 115200, timeout=0.5)
s.dtr = False
s.rts = False

print("Opening serial, reading for 10 seconds...")
t = threading.Thread(target=reader)
t.start()

time.sleep(10)
stop = True
t.join()
s.close()

result = b''.join(data)
print(f"\nTotal: {len(result)} bytes")
print(result.decode('utf-8', 'replace')[:3000])
