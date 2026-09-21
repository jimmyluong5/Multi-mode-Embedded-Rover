import cv2
import numpy as np
import serial
import time
from ultralytics import YOLO
SERIAL_PORT = "COM9"
BAUD_RATE = 2000000


model = YOLO("yolo26n.pt") #using this specific model





def main():
    print(f"Connecting to {SERIAL_PORT} at {BAUD_RATE} baud...")

    ser = serial.Serial()
    ser.port = SERIAL_PORT
    ser.baudrate = BAUD_RATE
    ser.timeout = 0.1
    ser.open()

    if not ser.is_open:
        print(f"Failed to open {SERIAL_PORT}.")
        return

    print("Connected! Streaming clean camera feed now... Press 'x' to quit.")

    buffer = bytearray()
    cv2.namedWindow("Rover Camera Feed", cv2.WINDOW_NORMAL)
    cv2.resizeWindow("Rover Camera Feed", 960, 720)

    # Flush any leftover partial bytes
    ser.reset_input_buffer()

    while True:
        data = ser.read(ser.in_waiting or 4096)
        if len(data) > 0:
            buffer.extend(data)

            # Look for the 4-byte sync header "IMG!"
            header_idx = buffer.find(b"IMG!")
            while header_idx != -1 and len(buffer) >= header_idx + 8:
                payload_len = int.from_bytes(buffer[header_idx + 4 : header_idx + 8], byteorder="little")

                # Sanity check: valid frame size between 1KB and 100KB
                if payload_len < 1000 or payload_len > 100000:
                    buffer = buffer[header_idx + 4 :]
                    header_idx = buffer.find(b"IMG!")
                    continue

                total_frame_len = header_idx + 8 + payload_len
                if len(buffer) >= total_frame_len:
                    jpeg_bytes = buffer[header_idx + 8 : total_frame_len]
                    buffer = buffer[total_frame_len :]

                    # Decode the exact frame
                    np_arr = np.frombuffer(jpeg_bytes, dtype=np.uint8)
                    frame = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)

                    # Display clean video without any text overlay
                    if frame is not None:
                        cv2.imshow("Rover Camera Feed", frame)

                    header_idx = buffer.find(b"IMG!")
                else:
                    break

            if header_idx > 4096:
                buffer = buffer[header_idx :]
            elif header_idx == -1 and len(buffer) > 8192:
                buffer = buffer[-2048 :]

        #click the x key to exit.
        if cv2.waitKey(1) & 0xFF == ord('x'):
            break

    ser.close()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
