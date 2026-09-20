import cv2 #imports the OpenCV library 
import numpy as np 
import serial 
import time

#every jpeg starts and ends with a two byte marker 

#FF D8 - 0xFF, 0xD8 (SOI) -  Start of Image

#ends with (EOI) - End of image._E_contra
#0xFF 0xD9 

SERIAL_PORT = "COM9"
BAUD_RATE = 2000000 #was 115200, was 2000000 , 921600

def main():
    print(f"Connecting to {SERIAL_PORT}...." )

    #open the serial port
    ser = serial.Serial()
    ser.port = SERIAL_PORT
    ser.baudrate = BAUD_RATE
    ser.timeout = 0.1 #wait 100ms max for the data so the loop doesn't freeze
    ser.open()

    #verify if the serial port is open 
    if not ser.is_open:
        print(f"Failed to open {SERIAL_PORT}.")
        return
    
    print("Connected! Streaming camera feed now")


    #create a buffer to store the jpeg bytes 
    #then we feed the jpeg decoder the bytes and it turns them into pixels for us to display on the laptop

    buffer = bytearray()


    #we can change the size of the image
    cv2.namedWindow("Rover Camera Feed", cv2.WINDOW_NORMAL)
    cv2.resizeWindow("Rover Camera Feed", 960, 720)

    while True: 
        #read the available bytes from the usb
        data =ser.read(ser.in_waiting or 4096) #read 1024 bytes at a time
        if(len(data) > 0): #if we have data
            buffer.extend(data) #then we add the data to the buffer

            #look for the start of the jpeg image
            i = buffer.find(b"\xff\xd8") #searches the buffer from byte 0 and tries to find the SOI bytes 
            #if it can't find it then it returns -1

            if i == -1:
                if len(buffer) > 2048:
                    buffer = buffer[-1024:] #keeps the last 1024 bytes.


            if i != -1:
                #look for the end marker 
                #j tries to find the EOI bytes, if it can't find it then return -1
                #we have the starting index by now, so we know where to start and try to find
                j = buffer.find(b"\xff\xd9",i)

                if j !=-1:
                    #place the jpeg bytes into the buffer from i to j+2 so the entire image
                    jpeg_bytes = buffer[i:j+2] #because j will stop 2 bytes early and we need to include those last two EOI bytes.
                    buffer = buffer[j+2:] #this deletes all the processed bytes from the front of the buffer
                    #clears memory 
                    if len(jpeg_bytes) < 500:
                        continue
                    


                    #places the raw bytes into numpy array with each byte as an unsigned 8 bit integer.
                    np_arr = np.frombuffer(jpeg_bytes, dtype=np.uint8)

                    #cv2.imdecode is the actual decoder and takes the compressed jpeg array and converts it 
                    #into a 2D grid of RGB pixels [240 rows, 320 columns, 3 color channels.]
                    frame= cv2.imdecode(np_arr, cv2.IMREAD_COLOR)

                    #if frame decoded successfully, show it
                    if frame is not None:
                        #this creates a desktop window and displays the image on the screen.
                        cv2.imshow("Rover Camera Feed", frame)
                    
        #exit if 'x' is pressed 
        #wait 1 ms, and 
        if cv2.waitKey(1) & 0xFF == ord('x'):
            break                    
        
    #close the serial port
    ser.close()
    cv2.destroyAllWindows() #closes the opencv desktop window


if __name__ == "__main__":
    main()