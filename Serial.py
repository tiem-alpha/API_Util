import serial
import time
import random
import struct

PORT = "COM5"
BAUD = 115200
SEND_INTERVAL = 0.03   # 30ms

ser = serial.Serial(PORT, BAUD, timeout=0.01)

print("Opened", PORT)

try:
    while True:
        # random data length
        data_len = random.randint(100, 200)

        # packet format: AA | len_hi len_lo | data... | BB
        packet = bytearray()
        packet.append(0xAA)

        # length 2 bytes big endian
        packet += struct.pack(">H", data_len)

        # data bytes (all 0x55)
        packet += bytes([0x55] * data_len)

        packet.append(0xBB)

        # send
        ser.write(packet)
        # print("Sent packet len:", len(packet))

        # check response if any
        resp = ser.read(1024)
        if resp:
            try:
                text = resp.decode('utf-8', errors='ignore')
            except:
                text = str(resp)
            print("RX:", text)

        time.sleep(SEND_INTERVAL)

except KeyboardInterrupt:
    print("Stopped")

finally:
    ser.close()
