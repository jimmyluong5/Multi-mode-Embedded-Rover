# Multi-mode Rover

> **Status: Project Complete (September 2026)** 🎉  
> A fully integrated multi-microcontroller autonomous rover featuring distributed wireless control, FreeRTOS preemptive scheduling, Time-of-Flight collision safety, and real-time computer vision "Follow-Me" tracking.

<img width="480" height="853" alt="line_following_github" src="https://github.com/user-attachments/assets/8277d7d8-333a-4ad6-8d29-b165df40b6e0" />

Rough Prototype of Line Following in mid-July



An multi-mode rover built around the **STM32G431KB**, using an 8-channel reflectance sensor array for autonomous line following, LSM6DS3 IMU for tilt control UART for diagnostics and control, and ESP-NOW for wireless communication.


The project began as a basic motor-control prototype and has gradually evolved into a full rover platform with autonomous navigation, sensor monitoring, multiple operating modes, and wireless control.
<p align="center">
  <img src="https://github.com/user-attachments/assets/a8743f6e-6503-4095-bb05-f7fdd0e41f4b" width="24%" />
  <img src="https://github.com/user-attachments/assets/1136528b-bf2d-42e9-91b3-132347ca7aca" width="24%" />
  <img src="https://github.com/user-attachments/assets/8ce91f7d-2d42-44e9-8486-3224fbb3c6df" width="24%" />
  <img src="https://github.com/user-attachments/assets/ef135034-5aff-4634-a7c1-6e8d8ae37eac" width="24%" />
</p>


## Features

* **Autonomous "Follow-Me" Target Tracking**: Powered by **Ultralytics YOLO (BoT-SORT)** and **OpenCV** over a 2 Mbaud camera stream with **EMA filtering** and Ackermann steering
* **FreeRTOS Preemptive Dual-Core Multitasking**: Pinned on the ESP32-S3 handheld transmitter isolating 40 Hz RF transmission from heavy SPI LCD rendering
* Autonomous line following using an **STM32G431KB**
* 8-channel reflectance sensing using the **QTRX-MD-08A**
* External ADC acquisition using the **MCP3208**
* UART-based control and diagnostics through **PuTTY**
* DC motor control using **FIT0484 motors** and a **TB6612FNG dual motor driver**
* Regulated power distribution using **LM2596 buck converters**
* Wireless multi-MCU communication across **ESP32-S3** nodes using **ESP-NOW**
* Real-time Time-of-Flight (ToF) obstacle detection and automatic emergency braking using the **VL53L1X** laser ranger on a **FireBeetle 2 ESP32-S3** node
* Custom 8-bit command packet system for wireless control
* Multiple diagnostic and control modes for testing individual subsystems

## Development Process

### 1. Motor Control

The first stage of the project focused on getting the prototype working.

Two **FIT0484 DC motors** were controlled through a **TB6612FNG dual motor driver**, with the STM32G431KB acting as the main controller.

The power system uses **LM2596 buck converters** to provide regulated voltages to the motors and embedded electronics.

Once reliable bidirectional motor control was established, the prototype was mounted onto a rough cardboard chassis for early testing.

<img width="480" height="853" alt="motors_github" src="https://github.com/user-attachments/assets/ca83976e-c8b5-4a69-80d2-1aaa2ccd849a" />




### 2. Cardboard Prototype

A temporary cardboard rover was constructed to test the complete prototype before moving to a permanent chassis.

This prototype made it possible to verify

* Motor direction and speed control
* Wheel placement and mechanical alignment
* Breadboard wiring
* Initial sensor placement
* Basic autonomous movement

The prototype successfully demonstrated that the embedded control system and prototype could operate together before the mechanical design was finalized.

<img width="480" height="853" alt="first_drive_github" src="https://github.com/user-attachments/assets/3769e857-5dfd-4aa8-bf13-f9b57ea0bf7f" />



### 3. Reflectance Sensor Integration and UART Sensor Calibration and Diagnostics


The next stage introduced the **QTRX-MD-08A 8-channel reflectance array** for line detection.

The sensor outputs were connected to an external **MCP3208 ADC**, allowing the STM32G431KB to acquire all eight reflectance channels in real time.

Each channel produces an analog voltage corresponding to the reflectivity of the surface underneath the sensor.

During testing

* Higher measured voltage corresponded to darker surfaces
* Lower measured voltage corresponded to lighter surfaces

The STM32 reads the MCP3208 measurements and uses the eight sensor values to determine the rover's position relative to the line.

A UART-based diagnostic interface was also created using **PuTTY** to assist with sensor calibration and subsystem testing.

The interface displays both the measured voltage and raw ADC value for each reflectance channel.

Example output

```text
--- Sensor Test Mode Active ---

CH0: 1.91V | ADC: 2376
CH1: 1.92V | ADC: 2391
CH2: 1.96V | ADC: 2442
CH3: 1.98V | ADC: 2466
CH4: 1.96V | ADC: 2434
CH5: 1.97V | ADC: 2453
CH6: 2.05V | ADC: 2552
CH7: 1.96V | ADC: 2440
```

Testing the array across different surfaces made it possible to determine usable thresholds and calibrate the line-following algorithm.

The UART interface also provides multiple operating and diagnostic modes including

* Servo control
* DC motor testing
* Reflectance voltage monitoring
* Autonomous line-following mode

This made it possible to test individual subsystems without repeatedly modifying the firmware.



<img width="520" height="355" alt="image" src="https://github.com/user-attachments/assets/7266f23a-6372-4940-9bef-93d4786f03ce" />
<img width="492" height="395" alt="image" src="https://github.com/user-attachments/assets/38084f03-c8ef-4abd-8d70-750e41a66246" />




### 4. Autonomous Line Following

After calibrating the reflectance sensors, autonomous line following was implemented on the STM32G431KB.

The rover continuously samples all eight channels through the MCP3208 and uses the sensor distribution to determine how far the rover has moved away from the desired path.

Motor commands are then adjusted in real time to steer the rover back toward the line.

This resulted in a complete sensing and control loop:

```text
QTRX-MD-08A
      |
   MCP3208
      |
STM32G431KB
      |
Line-Following Logic
      |
  TB6612FNG
      |
 FIT0484 Motors
```

### 5. CAD Design

After validating the rover's electronics and autonomous navigation on the initial prototype, the mechanical components for the final rover were designed in CAD.

The CAD design focused on creating a rigid and modular platform for the drivetrain, electronics, steering system, and other mechanical components. Designing the rover as an assembly also made it possible to verify component placement, mechanical clearances, and overall dimensions before fabrication.

#### Front Steering Mechanism

The front steering assembly was designed as a separate mechanical system before being integrated into the complete rover. The mechanism allows both front wheels to steer together while maintaining a compact geometry at the front of the chassis.

The assembly was modeled in CAD to verify the movement of the steering components, wheel alignment, and clearance between moving parts.

<img width="900" height="203" alt="front_assembly_zoomed" src="https://github.com/user-attachments/assets/be88c435-6175-4b57-9284-9151091c387f" />

  <i>CAD model demonstrating the rover's front steering assembly.</i>
</p>

#### Main Chassis and Component Layout

The main chassis provides the structural foundation for the rover and supports the motors, wheels, electronics, power system, and cargo platform.

Component placement was considered during the design process to keep the rover compact while maintaining enough space for wiring, assembly, and access to the electrical hardware.

#### Complete Rover Assembly

After designing the individual mechanical components, they were combined into a complete CAD assembly. This allowed the overall rover geometry to be evaluated before the parts were manufactured.

The completed assembly was used to check wheel placement, steering clearance, chassis dimensions, and the positioning of the rover's electrical and mechanical components.

<img width="532" height="301" alt="whole car" src="https://github.com/user-attachments/assets/edb7affe-be3b-4195-935e-133e81508760" />

  <i>Complete CAD assembly of the autonomous warehouse rover.</i>
</p>

The CAD models used for the rover can be found in the [`CAD Models`](./CAD%20Models) directory.

### 6. 3D-Printed Chassis

After validating the electronics and navigation system on the cardboard prototype, the rover was transferred to a custom **3D-printed chassis**.

The chassis provides mounting for

* DC motors
* Wheels
* Breadboard and embedded electronics
* Power system
* Reflectance sensor array
* Additional mechanical components

This provided a much more rigid and repeatable platform for continued development.
<img width="960" height="1280" alt="image" src="https://github.com/user-attachments/assets/c35e2f71-1075-40d3-a830-de78924b504b" />
<img width="960" height="1280" alt="image" src="https://github.com/user-attachments/assets/773da8c9-128c-478e-ad3e-9cb9869b7452" />


### 7. Servo-Based Suspension Testing

After transferring the rover to the 3D-printed chassis, I tested a **servo-actuated front suspension system** using an SG90 servo.

The servo was used to adjust the front of the chassis and verify that the suspension mechanism could move reliably under control from the STM32.

A short demonstration clip is included below.


<img width="480" height="853" alt="servo_github" src="https://github.com/user-attachments/assets/aa3a12b8-8584-4048-a9af-15f80190c2fd" />


The test helped validate the mechanical design before continuing with wireless control and additional rover integration.


### 8. ESP-NOW Wireless Communication

The current stage of the project focuses on wireless communication between two **ESP32-S3 modules**.

A custom command protocol is being implemented on top of **ESP-NOW**.

Button inputs on the transmitter are active-low. When a button is pressed, the corresponding command bit is set inside an **8-bit command packet**.

```text
Button Input
    ↓
GPIO Read
    ↓
8-Bit Command Packet
    ↓
ESP-NOW Transmission
    ↓
Receiver ESP32-S3
    ↓
Packet Decoding
    ↓
Command Execution
```


<img width="480" height="853" alt="esp32_github" src="https://github.com/user-attachments/assets/15d5e751-9ac8-4944-949a-a5fd62a3ef1e" />



The receiving ESP32-S3 unpacks the command byte and executes actions depending on which bits are active.

Initial testing has successfully demonstrated wireless command transmission and execution using LEDs.


### 9. Wireless Button Controller Prototype

After successfully establishing ESP-NOW communication between the two ESP32-S3 modules, I built a physical **button controller prototype** to provide direct wireless input to the rover.

The controller uses an **ESP32-S3** connected to multiple push buttons. Each button represents a different rover command and is read as an active-low GPIO input.

When a button is pressed, the ESP32-S3 updates the corresponding bit in the **8-bit command packet** and transmits the packet wirelessly using ESP-NOW.

The initial controller was assembled on breadboards to allow the button layout, wiring, and firmware to be tested and modified easily.

The prototype successfully demonstrated

* Reliable detection of multiple button inputs
* Active-low GPIO input handling
* Generation of the 8-bit command packet
* ESP-NOW transmission from the handheld controller
* Wireless reception and command decoding on the second ESP32-S3
* LED-based verification of transmitted commands
* Simultaneous handling of multiple command inputs

```text
Push Buttons
     ↓
ESP32-S3 GPIO
     ↓
8-Bit Command Packet
     ↓
   ESP-NOW
     ↓
Receiver ESP32-S3
     ↓
Command Decoding
     ↓
Rover Control
```

<img width="1920" height="2560" alt="photo_2026-08-21_12-02-11" src="https://github.com/user-attachments/assets/2219012e-3b85-4ee7-a2c4-481e44adae1c" />


This prototype confirms that the wireless controller architecture works as intended and provides a functional input device for the next stage of rover integration.

The next step is to connect the receiver ESP32-S3 to the rover control system so that button commands can directly control functions such as driving, operating modes, and additional rover mechanisms.

#### Current Status Update

Add the following under **Completed**

* Wireless button controller prototype
* 8-bit button command packet transmission
* Multi-button input testing

The wireless controller can now be used as the basis for full manual rover control alongside the autonomous line-following system.


### 10. Bidirectional Serial Monitoring and Wireless Command Debugging

After completing the wireless button controller prototype, I expanded the debugging interface so that both the transmitter and receiver ESP32-S3 modules can display controller activity through their respective serial connections.

On the transmitter side, button presses are detected from the active-low GPIO inputs, encoded into the 8-bit command packet, and displayed through the serial console. This makes it possible to verify that the correct command is being generated before it is transmitted wirelessly.

The packet is then sent over ESP-NOW to the receiver ESP32-S3. The receiver decodes the individual command bits and independently displays the interpreted rover command through its own serial console.

Using PuTTY, I can therefore monitor either side of the wireless connection and verify commands such as

MANUAL MODE
AUTONOMOUS MODE
INCREASING SPEED
DECREASING SPEED
STOP

<img width="987" height="726" alt="photo_2026-08-23_20-24-49" src="https://github.com/user-attachments/assets/f57871b0-3ed2-4805-978f-858f2236b0fb" />


The controller also supports an 8-bit speed value ranging from 0–255, with the UP and DOWN buttons adjusting the requested rover speed in approximately 5% increments.

Push Buttons
     ↓
     
Transmitter ESP32-S3
     ↓
Button Detection
     ↓
     
Serial Debug Output
     ↓
     
8-Bit Command Packet
     ↓
     
   ESP-NOW
     ↓
     
Receiver ESP32-S3
     ↓
     
Command Decoding
     ↓
     
Serial Debug Output
     ↓
     
MANUAL / AUTO / SPEED / STOP

Having serial output available on both ESP32-S3 modules provides visibility into both sides of the communication system. The transmitter output verifies that controller inputs are being correctly detected and encoded, while the receiver output verifies that the same commands are successfully transmitted, received, and decoded.

This significantly simplifies debugging because communication problems can be isolated to either the controller input stage, ESP-NOW transmission, or receiver-side command decoding.



<img width="538" height="956" alt="Adobe Express - IMG_5109" src="https://github.com/user-attachments/assets/989fd5d0-0708-4b79-a95e-a0d8b2106800" />



The wireless controller now supports both LED-based command verification and real-time serial debugging, providing a more reliable way to confirm that ESP-NOW packets are being received and interpreted correctly.

### 11. Full Rover Integration & Multi-Subsystem Control

With the 3D-printed chassis, power distribution, and core firmware validated, the rover has progressed to a **fully integrated, multi-mode platform**. All critical subsystems are now orchestrated directly by the STM32G431KB state machine:

* **Integrated Steering & Suspension**: The front servo suspension and steering mechanism is directly integrated into the drive loop, enforcing calibrated limits (45° to 135°) with dedicated combined driving modes (front active steering + rear differential motor thrust).
* **Unified UART Telemetry & Diagnostic Interface**: An interactive, non-blocking serial dashboard allows on-the-fly mode switching between:
  * `[m]` Motor Control Mode (Differential Drive)
  * `[c]` Combined Control System (Active Steering + Rear Motors)
  * `[a]` Autonomous PID Line Following
  * `[s]` Steering Servo Calibration (45°–135°)
  * `[t]` Stepper Motor Positioning
  * `[p]` Piezo Speaker / Buzzer Frequency Testing
  * `[v]` / `[n]` Real-Time ADC Reflectance & Surface Classification


### 12. Custom ESP32-S3 Wireless Handheld Controller & Real-Time Telemetry Dashboard
<img width="1920" height="2560" alt="photo_2026-09-06_17-12-59" src="https://github.com/user-attachments/assets/a79483f9-5157-4bf6-b995-1d22fd751318" />
<img width="1920" height="2560" alt="image" src="https://github.com/user-attachments/assets/eae1eeb8-ab55-4b0b-a11c-10c700d05e61" />



The communication architecture establishes a complete, closed-loop bidirectional data link between the handheld transmitter controller, the on-rover ESP32-S3 receiver, and the main **STM32G431KB** robot brain:

```text
+------------------------------------+             +----------------------------------+             +----------------------------------+
|    Handheld ESP32-S3 Transmitter   |             |     Rover ESP32-S3 Receiver      |             |       STM32G431KB Robot MCU      |
|                                    |   ESP-NOW   |                                  |    UART1    |                                  |
| - Analog Joystick (ADC1)           | ----------> | - Decodes ESP-NOW data_packet_t  | ----------> | - LPUART1 RX (PA3): 0xAA + packet|
| - 5-Way Button Matrix (Mode/Speed) |   (2.4GHz)  | - Forwards 0xAA + packet via TX  |  (Pin 42)   | - Drives TB6612FNG Dual Motors   |
|                                    |             |                                  |             |                                  |
| - 3.2" ILI9341 Diagnostics Display | <---------- | - Forwards robot_status_t via TX | <---------- | - LPUART1 TX (PA2): 0xAA + status|
|   (Live Speeds, Ticks, RTOS stats) |   ESP-NOW   | - Captures 0xAA + status via RX  |  (Pin 2)    | - DWT Timer Benchmarks & Metrics |
+------------------------------------+             +----------------------------------+             +----------------------------------+
```

### 13. Inter-MCU UART Bridge & Hardware Verification (ESP32-S3 <-> STM32G431KB)

To complete the end-to-end communication pipeline, dedicated hardware UART communication was established and verified between the rover's on-board **ESP32-S3 Receiver** and the **STM32G431KB** microcontroller:

* **Hardware Pinout**:
  * **ESP32-S3 Pin 42 (UART1 TX)** --> **STM32 PA10 (USART1 RX / D1)** @ 115,200 baud
  * **ESP32-S3 Pin 2 (UART1 RX)** <-- **STM32 PA9 (USART1 TX / D0)** @ 115,200 baud
  * **ST-Link Debug Console**: **STM32 PA2 (TX) / PA3 (RX)** via LPUART1 @ 115,200 baud (keeps PuTTY console communication completely isolated from rover control traffic).
  * **Common Ground (GND)** connected across all modules with regulated 5V buck power distribution.

### 14. Real-Time Proportional Manual Control & Closed-Loop Wireless Driving

The manual driving implementation provides responsive, proportional throttle and differential steering mapped directly from the analog 2D joystick on the handheld controller to the rover's TB6612FNG motor driver:

* **Joystick Mixing & Proportional Control**:
  * Filtered joystick X/Y coordinates from SAR ADC1 are converted into differential drive PWM duty cycles (`0%` to `100%`) for smooth acceleration, reverse, and spot-turning.
  * Real-time deadband filtering ($\pm 25$ ADC counts) prevents unintended motor creep when the spring-centered joystick is resting at neutral.
* **Safety Watchdog & Inactivity Alarm**:
  * An integrated 5-second inactivity watchdog trips into fail-safe mode if wireless packets or user input stall, sounding an audible 8-beep alarm pattern on the piezo buzzer and automatically disarming the motors.
* **Live Return Diagnostics**:
  * The STM32 sends back 20 Hz binary telemetry frames (`0xAA + robot_status_t`) carrying live DWT superloop metrics (CPU load, latency, jitter, loop frequency) and link quality parameters directly overlaid on the 3.2" TFT LCD.

<p align="center">
  <img src="./assets/telemetry_diagnostics_live.jpg" width="85%" alt="Live 4-Quadrant Diagnostic Dashboard on Handheld Controller" />
  <br>
  <i>Handheld 3.2" TFT LCD displaying real-time 4-quadrant diagnostics: Live Joystick coordinates, Differential Motor PWM, ESP-NOW Link Quality, and STM32 Superloop Performance.</i>
</p>

<p align="center">
  <!-- Manual Driving Demonstration GIF -->
  <img src="./assets/manual_drive_demo.gif" width="85%" alt="Manual Rover Driving Demonstration" />
  <br>
  <i>Live demonstration: Real-time proportional manual joystick control and responsive wireless maneuvering.</i>
</p>





https://github.com/user-attachments/assets/3c32494a-2592-41dd-a6e3-7c65f42f0c67





### 15. Autonomous Mode with Handheld UI & Live Sensor Array Dashboard

Building upon the initial prototype, this stage introduced a dedicated autonomous operating mode on the handheld controller alongside advanced navigation and recovery algorithms on the STM32:




https://github.com/user-attachments/assets/5376047f-b03f-42eb-b190-9bbf119622eb




#### 1. Handheld UI & Live 8-Channel Reflectance Visualizer (`PAGE_AUTO`)
When switching into **Autonomous Mode**, the controller renders an industrial visualizer that mirrors the rover's optical tracking in real time at ~30 FPS:
* **Dual-Tier Sensor Display**: Renders 8 vertical bars (Channels 1 to 8) and 8 trapezoidal ground-perspective tiles with bright yellow outlines.
* **Live Binary Fill**: Channels detecting dark line turn **Solid Pitch Black** (`0x0000`), while light background renders **White** (`0xFFFF`), updated directly from incoming `robot_packet.lineSensors` telemetry.
* **Live Speed Stepping**: Displays the commanded autonomous speed (e.g., `50%`) in Electric Cyan, with on-the-fly 5% speed adjustments via the D-pad buttons.
* **Run / Stop Safety State**: Center button toggles between **▶ RUNNING** (Green) and **■ STOPPED** (Red), automatically disarming the drive motors when paused or leaving the mode.

#### 2. Advanced Control & Lost-Line Recovery Algorithms (STM32)
* **Lead Steering & Curvature Anticipation**: Combines proportional-derivative control with rate-of-error anticipation to front-load steering servo corrections and eliminate corner turn-in lag.
* **Dynamic Corner Braking**: Automatically throttles inner-wheel PWM and reduces base velocity based on track curvature to prevent corner drift, rollover, and wheel skid.
* **Trajectory Memory & Lost-Line Recovery**: Maintains historical sensor states (`prev_channel`) so that if line continuity is lost, the rover references its last departure trajectory vector to steer back onto the track automatically.
* **Intersection Pass-Through**: Initiates a timed straight-line pass-through when 5+ sensors trigger simultaneously, navigating warehouse grid cross-junctions without false turns.

### 16. Real-Time IMU Gesture & Tilt Control Mode (`PAGE_IMU`)




https://github.com/user-attachments/assets/bebed1e2-1b77-4877-b2ca-f1f811145993




Building upon manual joystick and autonomous line-following modes, this stage introduced a hands-free **IMU Gesture & Tilt Control Mode**, integrating a 6-DoF **LSM6DS3** accelerometer and gyroscope on the handheld transmitter to steer and throttle the rover via wrist orientation:

#### 1. Handheld UI & Calibrated Crosshair Attitude HUD
When switching to **IMU Mode**, the 3.2" TFT LCD renders a dedicated attitude HUD powered by the double-buffered SPI DMA graphics engine (~35–40 FPS):
* **Dynamic 2D Tilt Reticle**: Real-time crosshair dot tracks pitch and roll within a calibrated boundary box (`X [40..130]`, `Y [139..224]`), visually indicating hand orientation relative to the neutral center `(85, 181)`.
* **Attitude Telemetry**: Real-time calculated **Pitch (°)** and **Roll (°)** angle values displayed in dedicated diagnostic readouts on the right.
* **Speed Mode Throttle Ceiling**: Displays current PWM base speed limit (`0%` to `100%`), adjustable on the fly in 5% increments via the D-pad buttons.
* **Safety Run/Stop Interlock**: Center button toggles between **RUNNING** (Green) and **STOPPED** (Red), locking motor and steering outputs to neutral zero when disarmed, in menus, or within the central ~7° deadband.
* **Optimized Graphics Pipeline**: String conversions and coordinate layouts are pre-calculated once per frame at `(0, 0)` rather than per-pixel, eliminating ~230,000 redundant `snprintf()` calls per second and maximizing display responsiveness.

#### 2. Closed-Loop Kinematics & Ackermann Steering Mapping (STM32)
The STM32 parses the incoming 40 Hz IMU telemetry packets (`packet.accel_x` and `packet.accel_y`) and maps wrist orientation directly to vehicle actuation:
* **Proportional Throttle (Pitch / Accel Y)**: Forward/backward tilt is normalized (±3000 counts) and scaled against the active speed ceiling into hardware timer PWM (`0` to `999` counts on `TIM1`/`TIM17`), providing progressive gas-pedal acceleration.
* **Proportional Steering (Roll / Accel X)**: Lateral controller tilt is normalized (±3000 counts) and mapped to the front suspension steering servo (`SERVO_ANGLE_CENTER ± 35°`, clamped between `55°` and `125°`), pivoting the front wheels in direct proportion to wrist roll while the rear motors drive through the turn.

### 17. Vision & ToF Obstacle Avoidance Subsystem (FireBeetle 2 ESP32-S3 Node)








https://github.com/user-attachments/assets/b8a65a7d-91a0-4f34-ae63-236313951bae






To expand the rover beyond static track navigation into intelligent interactive autonomy, this phase introduces a third microcontroller node: the **DFRobot FireBeetle 2 ESP32-S3 (N16R8)**. Mounted directly to the front of the rover chassis, this dedicated AIoT node integrates an **OV2640 DVP Camera** and an **ST VL53L1X Time-of-Flight (ToF)** laser ranging sensor to power dynamic obstacle braking and an upcoming autonomous **"Follow-Me"** mode.

```text
  [ ST VL53L1X ToF ] ──(I2C @ 400kHz)──┐
                                       ├──► [ FireBeetle 2 ESP32-S3 ]
  [ OV2640 Camera ]  ──(DVP 8-bit bus)─┘         (Camera & Range Node)
                                                         │
                                                  (ESP-NOW 30 Hz)
                                                         ▼
                                                [ ESP32-S3 Receiver ]
                                                         │
                                                    (UART 115.2k)
                                                         ▼
                                                [ STM32G431KB Brain ]
                                                (Kinematics & Motors)
```

#### 1. Hardware Architecture & Memory Pipeline
* **Microcontroller**: DFRobot FireBeetle 2 ESP32-S3 featuring dual 240 MHz Xtensa LX7 cores, 16MB Quad-SPI Flash, and **8MB Octal PSRAM** (`OPI`).
* **High-Bandwidth Octal PSRAM Enablement**: Configured native ESP-IDF build flags (`-DBOARD_HAS_PSRAM`, `-DCONFIG_SPIRAM_MODE_OCT=1`) to allocate high-resolution camera DMA frame buffers without exhausting internal SRAM.
* **Component-Managed Driver Architecture**: Configured via the pure ESP-IDF Component Registry (`idf_component.yml`), eliminating bulky Arduino runtime dependencies:
  * `espressif/esp32-camera (^2.1.7)`: Hardware-accelerated 8-bit DVP camera driver.
  * `espressif/esp_jpeg (^1.3.1)`: `TJpgDec` decoder for rapid frame decompression in computer vision preprocessing.
  * `grrtzm/vl53l1x_library (^0.3.1)`: Pure C driver leveraging ESP-IDF 5.x `i2c_master` bus architecture.

#### 2. Time-of-Flight (ToF) Distance Sensing Implementation
* **ST VL53L1X Laser Ranger**: Emits invisible 940 nm VCSEL laser pulses to compute millimeter-accurate distances independent of target surface color or reflectance.
* **Bus Multiplexing**: Interfaced over `I2C_NUM_0` on **GPIO 1 (SDA)** and **GPIO 2 (SCL)** at 400 kHz with internal pull-ups, coexisting cleanly on the physical header with the camera's SCCB configuration bus without address collision (`0x29` 7-bit ToF address vs `0x30` camera address).
* **Calibrated 33 ms (~30 Hz) Long Distance Mode**: Configured the macro timing and inter-measurement budget to **33 ms**, providing a 3× faster reaction time than standard 10 Hz sensors while maintaining dependable 2–3 meter obstacle detection under ambient room lighting.
* **Data Integrity Filtering**: Implemented range-status validation (`r.status == 0`) with timeout protections, discarding optical ambient noise and returning `0xFFFF` on invalid frames.

#### 3. Low-Latency ESP-NOW Wireless Bridge
* **Station Mode RF Pipeline**: Initialized NVS and the Wi-Fi radio stack in station mode (`WIFI_MODE_STA`) locked to Channel 1, establishing an ad-hoc, sub-2ms point-to-point wireless link directly to the rover's on-board receiver (`AC:27:6E:A2:87:5C`).
* **Binary Telemetry Packet (`0xBB + tof_packet`)**: Packets are formatted with an identification sync byte (`0xBB`) to differentiate range telemetry from the controller’s joystick packets (`0xAA`), streaming real-time distance and sensor health at ~30 Hz.

#### 4. Architecture & Roadmap Towards the "Follow-Me" Feature
The FireBeetle node forms the sensory foundation for the rover's upcoming intelligent tracking modes:
1. **Active Obstacle Detection & Dynamic Motor Interlock (Fully Integrated)**:
   * **Real-Time Transmission**: Distance packets (`0xBB + tof_packet_t`) stream over ESP-NOW at 30 Hz to the rover's receiver, which forwards them over USART1 (`PA10`) to the STM32.
   * **Collision Safety Envelope**: If the forward distance drops below the threshold (`< TOF_MAX_DISTANCE`, calibrated to 150–400 mm), the STM32 sets `obstacle_flag = true` and cuts motor drive via `Motor_Stop()` across both **Manual Joystick** and **IMU Gesture Tilt** modes.
   * **Seamless Forward Recovery**: When the obstacle is removed (or moves beyond range `0xFFFF`), the camera continues transmitting `status = 1`, automatically clearing `obstacle_flag = false` and instantly restoring forward throttle.
2. **Visual Target Centroid Acquisition**: Using the OV2640 camera, the ESP32-S3 will run lightweight color-blob and centroid tracking to calculate the target's horizontal pixel displacement (`target_x_offset`).
3. **Dual-Loop Follow-Me Control**:
   * **Distance Regulation (Longitudinal PID)**: The ToF reading feeds a distance PID controller on the STM32 to modulate rear motor throttle, maintaining a steady 400–500 mm gap from a walking user or moving target.
   * **Steering Alignment (Lateral PID)**: The camera's horizontal offset feeds the Ackermann front servo, steering the wheels left or right to keep the target centered in the field of view.


### 18. Autonomous Computer Vision & YOLO "Follow-Me" Tracking Pipeline




https://github.com/user-attachments/assets/8dc2a665-b75a-4082-97ff-59c3671479e8



To achieve true interactive autonomy, a real-time computer vision "Follow-Me" pipeline was engineered. The front-mounted camera node streams high-throughput video to host perception models in Python, which calculate proportional steering vectors to pursue moving targets dynamically:

```text
  [ OV3660 / OV2640 ] ──> [ ESP32-S3 Camera Node ]
                                 │
                         (2 Mbaud Serial Stream)
                         [ "IMG!" + Length + JPEG ]
                                 ▼
              [ Host PC: Python OpenCV + Ultralytics YOLO ]
                                 │
              ┌──────────────────┴──────────────────┐
              ▼                                     ▼
     [ YOLO + BoT-SORT Tracker ]           [ Signal Conditioning ]
     (Multi-Object ID Tracking)           (Deadband + EMA α=0.3 + Hysteresis)
              └──────────────────┬──────────────────┘
                                 ▼
              [ Binary Command Packet: 0xCC + Steer + Locked ]
                                 │
                      (High-Speed Return Link)
                                 ▼
                      [ STM32 Steering Servo ]
                      (Real-Time Ackermann Lock)
```

#### 1. High-Speed Serial Video Streaming & Frame Framing
* **2,000,000 Baud Throughput**: Streamed compressed JPEG frames over high-speed USB-UART at 2 Mbaud to maintain high framerates and minimize transmission latency.
* **Robust Packet Synchronization (`IMG!`)**: Engineered a custom frame-delimiting protocol prefixing each frame with a 4-byte sync header (`IMG!`) followed by a 4-byte little-endian payload length.
* **Integrity Validation**: Implemented dual JPEG SOI (`0xFFD8`) and EOI (`0xFFD9`) marker verification in Python, immediately dropping corrupted or partial DMA frames before decoding.

#### 2. Real-Time Deep Learning Perception (YOLO & BoT-SORT)
* **Ultralytics YOLO Integration**: Deployed YOLO (`yolo26n.pt` / `yolov8n.pt`) for continuous object detection and target localization with confidence filtering.
* **Persistent Multi-Object Tracking (BoT-SORT)**: Utilized the BoT-SORT tracking algorithm (`tracker="botsort.yaml"`, `persist=True`) to maintain persistent track IDs across camera frame drops and temporary occlusions.

#### 3. Control Signal Conditioning & Noise Reduction
* **Proportional Steering Calculation**: Computed angular steering error relative to the horizontal frame center with full Ackermann steering lock (±48°).
* **Deadband Windowing**: Implemented a 25-pixel deadband around the frame center to eliminate servo chatter during straight-line tracking.
* **Exponential Moving Average (EMA) Filter**: Applied an EMA low-pass filter (α = 0.3) to smooth rapid bounding-box fluctuations and ensure fluid servo kinematics.
* **Hysteresis Thresholding**: Suppressed redundant serial transmissions if steering changes remained below 2°, dramatically reducing serial bus saturation.
* **Exponential Target Loss Decay**: If the target exits the field of view, the steering angle decays at 20% per frame toward center (`smoothed_steer *= 0.8`), preventing sharp snap-backs.

#### 4. Closed-Loop Command Packet
* Dispatches a compact 3-byte command packet (`[0xCC, steer_angle, target_found]`) back to the rover to steer the physical front wheels and throttle the drive motors toward the locked target.


## Hardware Interconnect (Receiver <-> STM32):
* **ESP32-S3 Pin 42 (UART1 TX)** --> **STM32 PA10 (USART1 RX / D1)** @ 115,200 baud
* **ESP32-S3 Pin 2 (UART1 RX)** <-- **STM32 PA9 (USART1 TX / D0)** @ 115,200 baud
* **ST-Link Debug Console**: **STM32 PA2 (TX) / PA3 (RX)** via LPUART1 @ 115,200 baud
* **Common Ground (GND)** connected across all modules with regulated 5V buck power distribution.

#### Packet Protocols:
1. **Control Packet (`0xAA + data_packet_t`)**: Transmitted at 40 Hz from transmitter to STM32, containing 5-bit tactile button masks, commanded speed (`0-255`), 12-bit analog joystick X/Y deflections, and operating mode (`Manual`, `Autonomous`, `IMU`).
2. **Telemetry Packet (`0xAA + robot_status_t`)**: Transmitted at 20 Hz from STM32 back to transmitter, delivering real-time CPU load percentage, loop rate (Hz), execution latency (ms), jitter (ms), and missed deadline counters to the controller's diagnostic UI.

To provide manual override, multi-mode switching, and live diagnostics for the rover, a dedicated handheld wireless controller was developed using a dual-core **ESP32-S3** (240 MHz) and a 3.2-inch **ILI9341 SPI TFT LCD (240×320)**.

The controller provides an interactive graphical user interface (GUI), live telemetry monitoring, and low-latency packet transmission over **ESP-NOW**.

#### 1. Hardware Architecture & Inputs
* **Microcontroller**: Freenove ESP32-S3 WROOM (16MB Flash, 8MB PSRAM, Dual Xtensa LX7 cores running at 240 MHz).
* **Display & Touch**: 3.2" 240×320 SPI LCD driven by an ILI9341 controller, sharing the SPI bus with an XPT2046 resistive touch controller.
* **Analog 2D Joystick**: Multi-sampled through SAR ADC1 (GPIO 4 and GPIO 5) with 16× oversampling, software deadband filtering, and axis normalization.
* **5-Way Tactile Button Matrix**: Multi-button input array with two-stage temporal debouncing for menu navigation, speed adjustments, and emergency stop.
* **Audio Feedback**: PWM buzzer generating audible confirmation tones on button interactions and a 5-second inactivity watchdog alarm.

```text
[ Analog Joystick (ADC1) ] ──┐
[ 5-Button Matrix (GPIO) ] ──┼──> [ ESP32-S3#1 Controller & Transmitter ] ──(ESP-NOW 2.4GHz)──> [ESP32#2 Rover Receiver ] ──(USART1 PA9/PA10)──> [ STM32G431KB ]
[ Piezo Speaker (LEDC)   ] ──┤      │ (FreeRTOS Core 0/1)                                                                                         │
[ ILI9341 240x320 LCD    ] ──┘      └──<── Live 4-Quadrant Diagnostic Telemetry Return <──────────────────────────────────────────────────────────┘
```

#### 2. Embedded Graphics Engine & Menu Animation
* **High-Speed JPEG Decoder (`tjpgd`)**: Decompresses embedded full-color JPEG assets directly from Flash memory into DMA-accessible RAM.
* **Ping-Pong SPI DMA Buffering**: Uses dual parallel line buffers to pipeline SPI transfers asynchronously while computing the next scanline on the CPU.
* **Main Menu UI**: Plays a 15-frame animated rover sequence with an active pulsing yellow hover cursor to select operating modes (**Manual Mode**, **Autonomous Mode**, **IMU Mode**).
* **Showcase Pages**: Dedicated QR code and documentation screens for GitHub and LinkedIn.

#### 3. Real-Time Manual Dashboard & 4-Quadrant Diagnostics
When entering **Manual Mode**, the controller renders an industrial diagnostic dashboard with live telemetry overlays across four dedicated quadrants:

1. **Quadrant 1: Joystick Data (Top Left)**:
   * **X / Y ADC**: 12-bit raw analog reading from SAR ADC1 (`0–4095`).
   * **X / Y norm**: Normalized deflection percentage (`-100%` to `+100%`) with center deadband filtering.
2. **Quadrant 2: Motor Data (Top Right)**:
   * **Left / Right PWM**: Real-time commanded throttle percentage dispatched to the TB6612FNG H-bridge drivers.
   * **Steering / Differential Drive**: Proportional steering mixing turning commands cleanly into left and right wheel speeds.
3. **Quadrant 3: Link Data (Bottom Left)**:
   * **Packets RX**: Total return telemetry frames successfully received.
   * **Packets Lost**: Real-time ESP-NOW packet drop rate (`0.0%`).
   * **Last packet**: Inter-packet arrival latency (`20 ms`).
   * **RSSI**: Receiver signal strength indicator in dBm (`-24 dBm`).
4. **Quadrant 4: STM32 Performance (Bottom Right)**:
   * **CPU Load**: Real-time superloop CPU utilization percentage measured by hardware DWT cycle counting.
   * **Latency**: Active loop execution latency measured to 0.01 ms precision.
   * **Jitter**: RFC 3550 exponential moving average loop jitter filter.
   * **Missed DL**: Missed loop deadline counter (execution taking >10 ms).
   * **Loop Rate**: Deterministic superloop update frequency (~184 Hz).

#### 4. Real-Time System Performance & RTOS Telemetry
To benchmark control loop determinism (comparing FreeRTOS preemptive multitasking vs bare-metal superloops), a dedicated telemetry module (`metrics.c` on ESP32 & `uart_control.c` on STM32) tracks real-time performance indicators:

| Telemetry Metric | Measurement Method | Typical Reading | Purpose |
| :--- | :--- | :--- | :--- |
| **CPU Load** | Active computation vs superloop budget | `~5–8%` | Verifies compute headroom |
| **Latency** | ARM Cortex-M4 DWT Cycle Counter (`CYCCNT`) | `0.01–0.05 ms` | Measures microcontroller active compute latency |
| **Jitter** | RFC 3550 Exponential Moving Average filter | `1.5–6.8 ms` | Tracks loop timing stability |
| **Missed DL** | Execution time exceeding 10 ms budget | `0` | Proves hard real-time scheduling guarantees |
| **Loop Rate** | Hardware period reciprocal (`1 / dt`) | `~100–185 Hz` | Confirms responsive control execution |
| **Link RSSI** | ESP-NOW WiFi MAC RX Control Metadata | `-24 to -48 dBm` | Verifies wireless link budget and antenna range |

#### 5. Wireless Communication Protocol (`ESP-NOW`) & Safety Watchdog
Control packets (`data_packet_t`) are packed into a compact binary structure and transmitted as 2.4 GHz peer-to-peer unicast packets to the receiver ESP32 on the rover:
```c

typedef struct __attribute__((packed)) {
    uint8_t  button_data;  // 5-bit tactile button mask
    uint8_t  speed;        // Commanded speed percentage (0–100%)
    uint16_t joystick_x;   // Filtered analog X deflection
    uint16_t joystick_y;   // Filtered analog Y deflection
    int16_t  accel_x;      // Filtered IMU X tilt (Roll / Steering)
    int16_t  accel_y;      // Filtered IMU Y tilt (Pitch / Throttle)
    int16_t  accel_z;      // IMU Z acceleration
    int16_t  gyro_z;       // IMU Z gyro rate
    uint8_t  mode;         // Active operating mode (Manual, Auto, IMU)
} data_packet_t;
```

* **5-Second Inactivity Fail-Safe**: If no user input or joystick deflection is detected for 5 seconds in Manual Mode, the transmitter automatically triggers an audible buzzer alarm pattern (`speaker_pattern(8, 75, 75)`), trips the fail-safe state, cuts motor drive output to neutral stop, and returns safely to the main menu.

## Hardware

| Component       | Purpose                                   |
| --------------- | ----------------------------------------- |
| STM32G431KB     | Main rover controller                     |
| QTRX-MD-08A     | 8-channel reflectance sensor array        |
| MCP3208         | External ADC for reflectance measurements |
| FIT0484         | DC drive motors                           |
| TB6612FNG       | Dual DC motor driver                      |
| ESP32-S3 ×3     | Wireless transmitter and receiver         |
| LM2596          | Buck converters for voltage regulation    |
| SG90 Servo      | Mechanical actuation                      |
| LADDA Batteries | Main power source                         |

## Software and Communication

* Embedded C
* STM32 firmware
* UART
* SPI
* ESP-NOW
* PuTTY
* 8-bit command packet protocol

## Roadmap & Next Steps

```text
Current: Full Rover + UART Telemetry
                ↓
Phase 1: ESP32-to-STM32 Bridge & Wireless Joystick Control
                ↓
Phase 2: IMU Orientation & Heading Stabilization
                ↓
Phase 3: Time-of-Flight (ToF) Collision Detection & Auto-Braking
```

### Phase 1: ESP32-to-STM32 Bridge & Wireless Joystick Controller
* **Inter-MCU Communication**: Bridge the receiver ESP32-S3 directly to the STM32G431KB via hardware UART, forwarding decoded wireless packets into real-time drive and mode commands.
* **Analog Joystick Transmitter**: Upgrade the handheld controller with a 2-axis analog joystick to provide smooth, continuous proportional throttle and steering rather than discrete button presses.

### Phase 2: IMU Integration & Dynamic Heading Stabilization
* Integrate a 6-DOF / 9-DOF **Inertial Measurement Unit (IMU)** via I2C/SPI on the STM32.
* Implement closed-loop attitude estimation and yaw-rate compensation to maintain straight-line tracking, prevent drift, and detect chassis tilt over uneven terrain.

### Phase 3: Time-of-Flight (ToF) Collision Detection
* Integrate forward-facing **Time-of-Flight (ToF) distance sensors** (e.g., VL53L0X / VL53L1X).
* Implement real-time proximity sensing with dynamic speed reduction and autonomous emergency braking (AEB) to avoid obstacles during both manual and autonomous line-following modes.

## Current Status Summary

> **Status: Project Complete (September 2026)** 🎉  
> All primary design goals and subsystems—including distributed multi-MCU wireless networking, FreeRTOS preemptive multitasking, Time-of-Flight collision safety, and real-time computer vision "Follow-Me" tracking—have been fully implemented, validated, and integrated.

### Completed
- [x] Bidirectional DC motor control with TB6612FNG & dual hardware PWM (TIM1_CH1 / TIM17_CH1)
- [x] 8-Channel reflectance array acquisition via MCP3208 SPI ADC
- [x] Autonomous line-following navigation with PID control
- [x] 3D-printed chassis assembly and mechanical integration
- [x] Servo-actuated steering & suspension control (45 deg - 135 deg limits)
- [x] Multi-subsystem UART diagnostic & control dashboard
- [x] Dual ESP32-S3 ESP-NOW wireless link with binary command packet protocol (40 Hz)
- [x] Dedicated dual UART routing on STM32 (USART1 on PA9/PA10 for ESP32 receiver, LPUART1 on PA2/PA3 for PC PuTTY console)
- [x] Handheld wireless controller with 3.2" 240x320 ILI9341 LCD, 2D joystick grid, and 4 diagnostic telemetry quadrants
- [x] Real-time proportional manual joystick driving with differential throttle mixing
- [x] 5-second inactivity fail-safe timeout watchdog with multi-tone audio alarm
- [x] Closed-loop STM32 DWT performance metrics & link telemetry transmission over ESP-NOW back to handheld transmitter LCD (20 Hz)
- [x] 6-DoF IMU gesture tilt control (LSM6DS3) with proportional throttle and Ackermann servo steering
- [x] Real-time Time-of-Flight (VL53L1X) laser obstacle avoidance and emergency braking integrated into STM32 motor pipeline across Manual & IMU modes
- [x] Preemptive FreeRTOS dual-core multitasking on ESP32-S3 transmitter (Core 0: 40 Hz RF transmission, Core 1: SPI LCD rendering)
- [x] Autonomous "Follow-Me" target tracking vision pipeline using Ultralytics YOLO (BoT-SORT) and OpenCV over a 2 Mbaud camera stream with EMA filtering and Ackermann steering

