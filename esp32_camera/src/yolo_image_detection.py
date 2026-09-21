from ultralytics import YOLO

#load the latest model 
model = YOLO("yolo26n.pt")

#just use the pre-trained model to identify this object
results = model("https://ultralytics.com/images/bus.jpg")

#display the results with bounding boxes
results[0].show()
"""  
#if you want to train the model 
results = model.train(data="coco8.yaml", epochs=100, imgsz = 640)
#the coco8.yaml is the data file, epochs is the time, imgsz is the image size"""


"""
results[0] represents the one camera frame at a time.

results
│
└── results[0]
    │
    ├── original image
    ├── detected boxes
    ├── classes
    ├── confidence scores
    └── show()

so like 
results[0].boxes 
results[0].classes
results[0].show() #which shows the result and displays the original image with all of the 
detections drawn on top.

results[0]
- can contain many different things.


results[0].boxes
- can contain many detected objects

results[0]
│
└── boxes
    ├── box 0 → person
    ├── box 1 → chair
    ├── box 2 → person
    └── box 3 → backpack


results[0]
results[1]
results[2]
-results[1] is the 2nd frame
-results[2] is the 3rd frame
-results[n] is the n-1 frame

if you gave multiple images
results = model([
    "image1.jpg",
    "image2.jpg",
    "image3.jpg"
])
then you would get 
results[0] → predictions for image1
results[1] → predictions for image2
results[2] → predictions for image3


but for the rover we only need 1 camera frame at a time 


so results[0].boxes[0] is the first image and the first detected image.


"""