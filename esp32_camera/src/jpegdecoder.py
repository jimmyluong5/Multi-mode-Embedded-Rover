import cv2
import numpy as np
import serial
import time
from ultralytics import YOLO
import struct


SERIAL_PORT = "COM9"
BAUD_RATE = 921600




model = YOLO("yolo26n.pt") #using this specific model can use any model

PACKET_FORMAT = "<BbB"
#we just need to determine the steering angle and send that to the receiver then to the stm32
#inputs are boolean target_found and offset, if the model detects if im left or right or centered.
def get_steering_angle(target_found, x1, x2, frame_width):
    if target_found == False:
        return 0 #0 degrees

    frame_center = frame_width /2.0

    mid_x = (x1+x2)/2.0
    offset = mid_x - frame_center

    deadband = 40
    max_steer = 30

    #clamp the steer angle
    if abs(offset) < deadband:
        steer_angle = 0

    else:
        steer_angle = int((offset/frame_center) * max_steer)

        #clamp between -30 and 30
        steer_angle = max(-max_steer, min(max_steer, steer_angle))
    return steer_angle

        
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

                    is_soi = (jpeg_bytes[0] == 0xFF and jpeg_bytes[1] == 0xD8)
                    is_eoi = (jpeg_bytes[-2] ==0xFF and jpeg_bytes[-1] == 0xD9)


                    if not is_soi or not is_eoi:
                        #print(f"[CORRUPT] Size: {len(jpeg_bytes)} / {payload_len} | SOI (FF D8): {is_soi} | EOI (FF D9): {is_eoi} | Last 4 bytes: {jpeg_bytes[-4:].hex()}")
                        header_idx = buffer.find(b"IMG!")
                        continue  # Skip decoding corrupted frames
                    else:
                        print(f"[CLEAN FRAME] {len(jpeg_bytes)} bytes | Valid SOI & EOI")
                    # Decode the exact frame
                    np_arr = np.frombuffer(jpeg_bytes, dtype=np.uint8)
                    frame = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)


                    # Display clean video without any text overlay
                    if frame is not None:
                        cv2.imshow("Rover Camera Feed", frame)
                    

                        #we do the model stuff here
                        results = model.track(
                            frame, 
                            #show = True, this opens its own window but we already have a window.
                            persist = True,
                            imgsz = 320,
                            tracker = "bytetrack.yaml", #can change this tracker.
                            verbose = False,
                            classes = [0]
                        )
                        actual_frame = results[0].plot() #the actual frame.


                        if results[0].boxes is not None and len(results[0].boxes) > 0:
                            #get the coordinates of the primary target.
                            #we found the target set the flag
                            target_found = True
                            box = results[0].boxes.xyxy[0].cpu().numpy()
                            x1, y1, x2, y2 = box

                            #calculate steering angle
                            steer_angle = get_steering_angle(target_found, x1, x2, frame.shape[1])
                        else:
                            target_found = False
                            steer_angle = 0

                        
                        #create 2 byte command packet
                        follow_packet = struct.pack(PACKET_FORMAT, 0xCC, steer_angle, 1 if target_found else 0)
                        status_text = f"Steer: {steer_angle:+03d} deg | Target: {'LOCKED' if target_found else 'SEARCHING'}"
                        cv2.putText(actual_frame, status_text, (20, 40),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0) if target_found else (0, 0, 255), 2)
                        cv2.imshow("Rover Camera Feed", actual_frame)
                            
                            



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
