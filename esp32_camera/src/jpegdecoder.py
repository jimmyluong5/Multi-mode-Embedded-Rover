import cv2
import numpy as np
import serial
import time
from ultralytics import YOLO
import struct

SERIAL_PORT = "COM9"
BAUD_RATE = 2000000

model = YOLO("yolo26n.pt")  # or "yolov8n.pt"

# Command packet: 0xCC (sync byte), steer_angle (-30 to +30 deg), target_found (1 or 0)
PACKET_FORMAT = "<BbB"

#global variable to store the previous smoothed angle
smoothed_steer = 0.0

def get_steering_angle(target_found, x1, x2, frame_width):

    #we finna apply an ema filter to get rid of the jitter
    global smoothed_steer

    
    if not target_found:
        smoothed_steer = smoothed_steer * 0.8
        return int(smoothed_steer)

    frame_center = frame_width / 2.0
    mid_x = (x1 + x2) / 2.0
    offset = mid_x - frame_center

    deadband = 25    # Tighter deadband (was 40) for quicker steering response
    max_steer = 48   # Increased from 30 to allow full Ackermann steering lock

    if abs(offset) < deadband:
        raw_steer = 0
    else:
        # Higher proportional gain: full lock when target is ~65% across the frame
        steer_ratio = offset / (frame_center * 0.65)
        raw_steer = int(-steer_ratio * max_steer)
        raw_steer = max(-max_steer, min(max_steer, raw_steer))
    alpha = 0.3
    #ema equation is EMA_today = (alpha * Price_today) + (1-alpha)*(EMA_yesterday)
    #ema_yesterday is the smoothed steer which is the previous value of the steering
    #price_today is the current steering that we calculate.all
    smoothed_steer = (alpha*raw_steer) + (1-alpha)*smoothed_steer
    return int(smoothed_steer)


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

    print("Connected! Streaming clean camera feed with YOLO tracking now... Press 'x' to quit.")

    buffer = bytearray()
    cv2.namedWindow("Rover Camera Feed", cv2.WINDOW_NORMAL)
    cv2.resizeWindow("Rover Camera Feed", 960, 720)

    # Flush any leftover partial bytes
    ser.reset_input_buffer()
    last_print_time = 0
    last_sent_steer = 0
    last_target_found = False

    while True:
        data = ser.read(ser.in_waiting or 4096)
        if len(data) > 0:
            buffer.extend(data)

            # Look for the 4-byte sync header "IMG!"
            header_idx = buffer.find(b"IMG!")
            while header_idx != -1 and len(buffer) >= header_idx + 8:
                payload_len = int.from_bytes(buffer[header_idx + 4 : header_idx + 8], byteorder="little")

                # Sanity check: valid frame size between 1KB and 500KB
                if payload_len < 1000 or payload_len > 500000:
                    buffer = buffer[header_idx + 4 :]
                    header_idx = buffer.find(b"IMG!")
                    continue

                total_frame_len = header_idx + 8 + payload_len
                if len(buffer) >= total_frame_len:
                    jpeg_bytes = buffer[header_idx + 8 : total_frame_len]
                    buffer = buffer[total_frame_len :]

                    # Validate JPEG markers
                    is_soi = (jpeg_bytes[0] == 0xFF and jpeg_bytes[1] == 0xD8)
                    is_eoi = (jpeg_bytes[-2] == 0xFF and jpeg_bytes[-1] == 0xD9)

                    if not is_soi or not is_eoi:
                        header_idx = buffer.find(b"IMG!")
                        continue  # Skip decoding corrupted frames

                    # Decode the exact frame
                    np_arr = np.frombuffer(jpeg_bytes, dtype=np.uint8)
                    frame = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)

                    if frame is not None:
                        # Run YOLO person tracking
                        results = model.track(
                            frame,
                            persist=True,
                            imgsz=480,
                            tracker="botsort.yaml", #was bytetrack.yaml, now bot sort tracker.
                            verbose=False,
                            classes=[67], 
                            #used to be 
                            # classes = [0] for person, 
                            # changed [67] - phone, 
                            # classes = [39] = bottle
                            #classes = [73] - book
                            conf = 0.30 #give a confidence threshold.
                        )
                        actual_frame = results[0].plot()

                        if results[0].boxes is not None and len(results[0].boxes) > 0:
                            target_found = True
                            box = results[0].boxes.xyxy[0].cpu().numpy()
                            x1, y1, x2, y2 = box
                            steer_angle = get_steering_angle(target_found, x1, x2, frame.shape[1])
                        else:
                            target_found = False
                            steer_angle = 0

                        # Create and send 3-byte command packet back to ESP32: [0xCC, steer_angle, target_found]
                        follow_packet = struct.pack(PACKET_FORMAT, 0xCC, steer_angle, 1 if target_found else 0)

                        hysteresis_threshold = 2 #two degrees can change later.
                        if (abs(steer_angle-last_sent_steer) >= hysteresis_threshold or (target_found != last_target_found)):
                            last_sent_steer = steer_angle
                            last_target_found = target_found

                            try:
                                ser.write(follow_packet)
                            except Exception as e:
                                print(f"Serial write error: {e}")

                        if time.time() - last_print_time > 0.5:
                            print(f"[TRACK] Target: {'LOCKED' if target_found else 'SEARCHING':<9} | Steer: {steer_angle:+03d} deg | Sent: {[hex(b) for b in follow_packet]}")
                            last_print_time = time.time()

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

        # Click the 'x' key to exit
        if cv2.waitKey(1) & 0xFF == ord('x'):
            break

    ser.close()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
