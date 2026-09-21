from ultralytics import YOLO

model = YOLO("yolov8n.pt")
import time

""" results = model.track(source="https://ultralytics.com/images/bus.jpg", show=True)
results[0].show()
#access the unique tracking IDs from the results
if results[0].boxes.id is not None:
    print(f"Detected Track IDs: {results[0].boxes.id.cpu().numpy()}")"""



#for actual tracking with a video we can do the below

#have a default tracking method, there are a ton of tracking methods
#results = model.track("https://www.youtube.com/watch?v=-9lP95Qo-I0", show=True, tracker = "bytetrack.yaml")
#then at the end after show True you can do show True, tracker = "bytetrack.yaml" o r
#or any tracker you want

#to exit press (q)
""" 
for result in model.track("https://www.youtube.com/watch?v=-9lP95Qo-I0",
stream=True, show=True, imgsz = 320, classes = [0], tracker = "bytetrack.yaml"):
    time.sleep(0.01) #sleep/pause for 50ms """


print(model.names)

#for result in model.track(frame, show = True, imgsz = 320, classes = [0], persist = True, tracker = "bytetrack.yaml"):
#classes = [0] is looking for people only.
# 
#  