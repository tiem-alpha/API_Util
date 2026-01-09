import serial
import time
import random
import msvcrt  

PORT = "COM5"
BAUD = 115200
sending_enabled = True
ser = serial.Serial(PORT, BAUD, timeout=0.01)

SEND_INTERVAL = 0.04  # seconds
last_send = time.monotonic()
last_print = time.monotonic()
total = 0
pre_total =0

def send_packet():
    length = random.randint(10, 50)

    length_h = (length >> 8) & 0xFF
    length_l = length & 0xFF

    payload = bytes([0xAA] * length)

    packet = bytes([0xAA, length_h, length_l]) + payload + bytes([0xBB])
    
    ser.write(packet)
    # print("Sent:", (length+4))
    return length + 4
print("UART started on", PORT)

try:
    while True:

        # ---- SEND PACKET PERIODICALLY ----
        if msvcrt.kbhit():
            key = msvcrt.getch().decode().upper()

            if key == 'E':
                sending_enabled = False
                print(">>> Sending Paused")

            elif key == 'S':
                sending_enabled = True
                print(">>> Sending Resumed")
        now = time.monotonic()
        if now - last_send >= SEND_INTERVAL and sending_enabled:
            total+= send_packet()
           
            last_send = now
        if now - last_print > 1:
            if pre_total != total:
                print("Sent: ",total)
                pre_total = total
            last_print = now
        # ---- READ IMMEDIATELY WHEN DATA ARRIVES ----
        data = ser.read(1024)
        if data:
            print(data)   # in dạng bytes nguyên bản

        # tránh chiếm CPU
        time.sleep(0.001)

except KeyboardInterrupt:
    ser.close()
    print("Closed")