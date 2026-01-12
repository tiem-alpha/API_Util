import serial
import time
import random
import msvcrt  

PORT = "COM5"
BAUD = 115200
sending_enabled = True
ser = serial.Serial(PORT, BAUD, timeout=0.01)

SEND_INTERVAL = 0.1  # seconds
last_send = time.monotonic()
last_print = time.monotonic()
total = 0
total_payload =0; 
pre_total =0
rx_buffer = bytearray()


def crc16_mcrf4xx(data: bytes):
    poly = 0x1021
    crc = 0xFFFF
    
    for b in data:
        b = int('{:08b}'.format(b)[::-1], 2)  # reflect input byte
        crc ^= (b << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ poly
            else:
                crc <<= 1
            crc &= 0xFFFF

    # reflect output
    crc = int('{:016b}'.format(crc)[::-1], 2)
    return crc & 0xFFFF

def send_packet():
    length = random.randint(10, 50)

    length_h = (length >> 8) & 0xFF
    length_l = length & 0xFF

    payload = bytes([0xAA] * length)

    crc_input = bytes([length_h, length_l]) + payload
    crc = crc16_mcrf4xx(crc_input)

    crc_h = (crc >> 8) & 0xFF
    crc_l = crc & 0xFF

    packet = bytes([0xAC, length_h, length_l]) + payload + bytes([crc_h, crc_l, 0xBB])

    ser.write(packet)
    # print("send ", packet)
    return len(packet), length

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
            a, b = send_packet()
            total += 1
            total_payload += b
            last_send = now
        if now - last_print > 1:
            if pre_total != total:
                print("Sent: ",total, "Payload: ", total_payload    )
                pre_total = total
            last_print = now
        # ---- READ IMMEDIATELY WHEN DATA ARRIVES ----
        # data = ser.read(1024)
        # if data:
        #     print(data)   # in dạng bytes nguyên bản
        data = ser.read(1024)
        if data:
            rx_buffer.extend(data)
            print("Received:", data)
                # nếu cần xử lý payload thì xử lý ở đây
                # tránh chiếm CPU
        time.sleep(0.001)

except KeyboardInterrupt:
    ser.close()
    print("Closed")