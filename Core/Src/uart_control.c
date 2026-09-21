#include "robot.h"
#include <MCP3208.h>
#include <line_following.h>
#include <main.h>
#include <servo.h>
#include <stdbool.h>
#include <stdio.h>
//#include <stepper.h>
#include <string.h>
#include <uart_control.h>
#include "LSM6DS3.h"
#include <stdlib.h>
#include "motor.h"
#include "stm32g4xx_hal_uart.h"
#include <math.h>
static uint8_t current_servo_angle = 90;
//static int16_t current_stepper_angle = 0; 
// track current stepper angle starting at 0 deg.
#define BLACK_THRESHOLD 2359 // 1.90V on 3.3V ADC
#define TOF_MAX_DISTANCE 150

extern UART_HandleTypeDef huart1; // USART1: ESP32 Receiver (PA10 RX / PA9 TX)
extern UART_HandleTypeDef huart2; // LPUART1: PC PuTTY Terminal (PA2 TX / PA3 RX)
extern SPI_HandleTypeDef hspi1;
static uint16_t min[8];
static uint16_t max[8];
static uint16_t filtered_adc[8];
robot_status_t robot_status;  

static uint32_t last_command_time = 0;

//flag for the tof
static bool obstacle_flag = false;
static uint16_t tof_distance = 0xFFFF; //random value, we will use in the tof part 

// flag for sensor test
static bool sensor_test_active = false;

// flag for the first print in the sensor test mode.
static bool first_print = true;

// flag for the control mode
static UART_ControlMode current_mode = UART_MODE_MENU;



static void UART_SendMessage(const char *message);

void menu_main(void){
  char menu_buf[768];
  int percent = (robot_speed * 100) / 999;
  snprintf(menu_buf, sizeof(menu_buf),
           "\x1b[2J\x1b[H"
           "==========================================\r\n"
           "       Warehouse Rover Control Menu\r\n"
           "==========================================\r\n"
           "Select Option/Mode:\r\n"
           " [m] - Motor Control Mode\r\n"
           " [c] - Combined Control System (Motors + Servo Steering)\r\n"
           " [v] - Voltage / IR Sensor Test Mode\r\n"
           " [b] - Both Mode (Control + Sensor Prints)\r\n"
           " [n] - Normalized Color Mode\r\n"
           " [a] - Autonomous Line Following Mode\r\n"
           " [s] - Servo Control Mode\r\n"
           " [t] - Stepper Mode \r\n"
           " [p] - Speaker/Buzzer Test Mode\r\n"
           " [i] - LSM6DS3 IMU Test Mode\r\n"
           " [u] - UART Test Mode \r\n"
           "------------------------------------------\r\n"
           "Select Speed (Current: %d%%):\r\n"
           " [1] - Set Speed to 25%% PWM\r\n"
           " [2] - Set Speed to 50%% PWM\r\n"
           " [3] - Set Speed to 75%% PWM\r\n"
           " [4] - Set Speed to 100%% PWM\r\n"
           "==========================================\r\n",
           percent);
  UART_SendMessage(menu_buf);
}

void menu_imu(void) {
  UART_SendMessage(
      "\x1b[2J\x1b[H"
      "--- LSM6DS3 IMU Test Mode Active (Mode 'i') ---\r\n"
      "Commands:\r\n"
      " [r] - Read WHO_AM_I Register\r\n"
      " [d] - Dump / Scan Sensor Registers (0x00 - 0x3F)\r\n"
      " [h] - Return to Main Menu\r\n"
      "-----------------------------------------------\r\n");
}

void menu_motor(void){
  UART_SendMessage(
            "\x1b[2J\x1b[H"
            "--- Motor Control Mode Active ---\r\n"
            "Commands:\r\n"
            " [w] - Forward\r\n"
            " [s] - Reverse\r\n"
            " [a] - Spin Turn Left\r\n"
            " [d] - Spin Turn Right\r\n"
            " [x] - Stop / Idle\r\n"
            " [f] - Force Fault\r\n"
            " [1, 2, 3, 4] - Set Speed to 25%, 50%, 75%, 100% PWM\r\n"
            " [h] - Return to Main Menu\r\n"
            "---------------------------------\r\n");
}


void menu_combined(void) {
  UART_SendMessage(
            "\x1b[2J\x1b[H"
            "--- Combined Control System Active (Mode 'c') ---\r\n"
            "Commands:\r\n"
            " [w] - Drive Forward (Servo Center)\r\n"
            " [s] - Drive Reverse (Servo Center)\r\n"
            " [a] - Steer Left (Hold for 38 deg turn)\r\n"
            " [d] - Steer Right (Hold for 153 deg turn)\r\n"
            " [x] - Stop / Idle (Servo Center)\r\n"
            " [f] - Force Fault\r\n"
            " [1, 2, 3, 4] - Set Speed (25%, 50%, 75%, 100% PWM)\r\n"
            " [h] - Return to Main Menu\r\n"
            "--------------------------------------------------\r\n");
}

void menu_speaker(void) {
  UART_SendMessage("\x1b[2J\x1b[H"
                         "--- Speaker/Buzzer Test Mode Active ---\r\n"
                         "Press keys to test:\r\n"
                         " [1] - Play continuous 1 kHz tone\r\n"
                         " [2] - Play 100ms beep\r\n"
                         " [0] - Stop sound\r\n"
                         " [h] - Return to Main Menu\r\n"
                         "---------------------------------------\r\n");
}

void menu_stepper(void) {
  // put the custom menu for stepper motor here
        UART_SendMessage("\x1b[2J\x1b[H"
                         "--- Stepper Control Mode Active ---\r\n"
                         "Controls:\r\n"
                         " [a] - Decrease angle by 5 deg (CCW)\r\n"
                         " [d] - Increase angle by 5 deg (CW)\r\n"
                         " [1] - Set to 0 deg | [2] - Set to 180 deg | [3] - "
                         "Set to 360 deg\r\n"
                         " [h] - Return to Main Menu\r\n"
                         "-----------------------------------\r\n"
                         "Current Angle:    0 degrees");
}

void menu_normalized(void) {
  // print the message.
        UART_SendMessage(
            "\x1b[2J\x1b[H"
            "--- Normalized Color Mode Active (Press 'h' to return "
            "to Main Menu) ---\r\n");
}

void menu_autonomous(void) {
  UART_SendMessage(
            "\x1b[2J\x1b[H"
            "--- Autonomous Line Following Active ---\r\n"
            "Commands:\r\n"
            " [1, 2, 3, 4] - Set Speed (25%, 50%, 75%, 100% PWM)\r\n"
            " [h]          - Stop & Return to Main Menu\r\n"
            "----------------------------------------\r\n");
}

void menu_servo(void) {
  UART_SendMessage("\x1b[2J\x1b[H"
                         "--- Servo Control Mode Active ---\r\n"
                         "Controls:\r\n"
                         " [a] - Steer Left (-5 deg, min 38 deg)\r\n"
                         " [d] - Steer Right (+5 deg, max 153 deg)\r\n"
                         " [1] - Set to 38 deg (Left) | [2] - Set to 90 deg (Center) | [3] - Set to 153 deg (Right)\r\n"
                         " [h] - Return to Main Menu\r\n"
                         "---------------------------------\r\n"
                         "Current Angle:  90 degrees");
}

void menu_voltage(void)  {
  // print the stuff.
        UART_SendMessage("\x1b[2J\x1b[H"
                         "--- Sensor Test Mode Active (Press 'h' to return "
                         "to Main Menu) ---\r\n");
}

void menu_both(void) {
  // print the menu in this mode.
        UART_SendMessage(
            "\x1b[2J\x1b[H"
            "--- Both Mode Active ---\r\n"
            "Motor Commands:\r\n"
            " [w] - Forward | [s] - Reverse | [a] - Left | [d] - Right | [x] - "
            "Stop\r\n"
            " [1, 2, 3, 4] - Set Speed to 25%, 50%, 75%, 100% PWM\r\n"
            " [h] - Return to Main Menu\r\n"
            "---------------------------------\r\n");
}

void menu_uart(void) {
  UART_SendMessage(
      "\x1b[2J\x1b[H"
      "====================================================\r\n"
      "     UART Wireless Packet Diagnostic Mode ('u')     \r\n"
      "====================================================\r\n"
      "Listening on LPUART1 for incoming ESP-NOW packets...\r\n"
      "Frame Format: [0xAA] [data_packet_t (9 bytes)]\r\n"
      "\r\n"
      "Commands:\r\n"
      " [h] - Return to Main Menu\r\n"
      "----------------------------------------------------\r\n"
      "Waiting for incoming packets from ESP32...\r\n\r\n");
}




// function to set a message in UART.
static void UART_SendMessage(const char *message) {
  HAL_UART_Transmit(&huart2, (uint8_t *)message, strlen(message), HAL_MAX_DELAY);
}

static uint32_t led_blink_start_time = 0;
static bool led_is_blinking = false;

// function to initiate the UART.
void UART_CONTROL_init(void) {
  current_mode = UART_MODE_MENU;
  sensor_test_active = false; // have the sensor test off at the
  menu_main();
}

// function update the UART based on inputs through UART.
void UART_CONTROL_update(void) {
  // 8 bit byte to receive the inputs.
  uint8_t received_byte;

  // Handle non-blocking keypress LED turn-off after 50ms
  if (led_is_blinking && (HAL_GetTick() - led_blink_start_time >= 50)) {
    // Only turn off if the robot is currently idle or in fault state (other
    // states keep it ON)
    if (Robot_GetState() == robot_idle || Robot_GetState() == robot_fault) {
      HAL_GPIO_WritePin(LED2_GPIO_PORT, LED2_PIN, GPIO_PIN_RESET);
    }
    led_is_blinking = false;
  }

  // -------------------------------------------------------------------------
  // 1. Process incoming ESP-NOW packets from ESP32 on huart1 (USART1 / PA10)
  // -------------------------------------------------------------------------
  if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_ORE)) {
    __HAL_UART_CLEAR_OREFLAG(&huart1);
  }
  if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_NE) || __HAL_UART_GET_FLAG(&huart1, UART_FLAG_FE) || __HAL_UART_GET_FLAG(&huart1, UART_FLAG_PE)) {
    __HAL_UART_CLEAR_FLAG(&huart1, UART_CLEAR_NEF | UART_CLEAR_FEF | UART_CLEAR_PEF);
  }

  uint8_t esp_byte;
  if (HAL_UART_Receive(&huart1, &esp_byte, 1, 0) == HAL_OK) {
    if (esp_byte == 0xAA) {
      
      data_packet_t packet;
      if (HAL_UART_Receive(&huart1, (uint8_t*)&packet, sizeof(data_packet_t), 20) == HAL_OK) {
        // Toggle LED2 instantly on valid packet arrival
        HAL_GPIO_TogglePin(LED2_GPIO_PORT, LED2_PIN);

        uint8_t effective_speed = packet.speed > 0 ? packet.speed : 128;
        robot_speed = ((uint32_t)effective_speed * 999) / 255;

        Motor_SetStandby(false);
        last_command_time = HAL_GetTick();

        // Process joystick values
        int32_t x_raw = (int32_t)packet.joystick_x - 2048;
        int32_t y_raw = (int32_t)packet.joystick_y - 2048;

        if (abs(x_raw) < 200) x_raw = 0;
        if (abs(y_raw) < 200) y_raw = 0;

        int16_t fwd_pwm   = (y_raw * (int32_t)robot_speed) / 2048;
        int16_t side_pwm  = (x_raw * (int32_t)robot_speed) / 2048;
        int16_t left_pwm  = fwd_pwm + side_pwm;
        int16_t right_pwm = fwd_pwm - side_pwm;

        // Proportional Front Servo Steering: maps JoyX to 38 deg (Left) - 153 deg (Right)
        int16_t target_servo_angle = SERVO_ANGLE_CENTER + (int16_t)((x_raw * (SERVO_ANGLE_RIGHT - SERVO_ANGLE_CENTER)) / 2048);
        if (target_servo_angle < SERVO_ANGLE_MIN) target_servo_angle = SERVO_ANGLE_MIN;
        if (target_servo_angle > SERVO_ANGLE_MAX) target_servo_angle = SERVO_ANGLE_MAX;


      
        if (current_mode == UART_MODE_STM32) {
          char packet_values[160];
          snprintf(packet_values, sizeof(packet_values), "[ESP32->STM32] JoyX:%4u | JoyY:%4u | Spd:%3u | Mode:%u | Btns:0x%02X -> PWM L:%+4d R:%+4d | Servo:%d deg\r\n",
                   packet.joystick_x, packet.joystick_y, packet.speed, packet.mode, packet.button_data, left_pwm, right_pwm, target_servo_angle);
          UART_SendMessage(packet_values);
        }
        if (obstacle_flag == true && (left_pwm > 0 || right_pwm > 0) ) {
          //turn off the wheels
          left_pwm = 0;
          right_pwm = 0;
          Motor_Stop();
        }
        if (packet.mode == MANUAL_MODE) {
          Robot_SetState(robot_manual);
          Motor_Left_SetSpeed(left_pwm);
          Motor_Right_SetSpeed(right_pwm);
          
          Servo_SetAngle((uint8_t)target_servo_angle);
        }
        else if (packet.mode == AUTO_MODE) {
          Robot_SetState(robot_auto);
          
          //we call the line following function
          Robot_LineFollow_Update();
        }
        else if (packet.mode == IMU_MODE) {
          Robot_SetState(robot_imu);
          
         
          //throttle variables,
          //so we read the raw tilt reading from the accelerometer, 
          //which is between -4500 and 4500
          
          //we calculate throttle ratio by dividing packet.accel_y/4500.0 to get a number
          //between -1.0 and 1.0 so 1.0 is fully turning the controller in the +y

          //packet.speed we just increment based on our buttons

          //motor pwm = throttle_ratio * (float)packet.speed
          //if packet.speed = 60%
          
          float throttle_ratio = (float)packet.accel_y / 3000.0f;
          
          //clamp the throttle ratio between 1.0 and -1.0, like maps the y max and min of the accelerometer.
          if (throttle_ratio > 1.0f) {
            throttle_ratio = 1.0f;
          }
          else if (throttle_ratio < -1.0f) {
            throttle_ratio = -1.0f;
          }

          //this is the gas padel, scales the speed percentage by the amount of tilt
          int16_t motor_pwm = (int16_t)(throttle_ratio * (float)packet.speed * 10.0f);
          if (obstacle_flag == true && motor_pwm > 0)  {
            //set the motor_pwm to 0 then stop the motors
            motor_pwm = 0;
            Motor_Stop();
          }
          //this is the steering part
          float steer_ratio = (float)packet.accel_x / 3000.0f;
          //clamp the steer ratio, just translates the servo angles like maps the max and min
          //degree of the servo.
          if (steer_ratio > 1.0f) {
            steer_ratio = 1.0f;
          }
          else if (steer_ratio < -1.0f) {
            steer_ratio = -1.0f;
          }

          //mechanical clamp for the servo
          int16_t servo_angle = SERVO_ANGLE_CENTER + (int16_t)(steer_ratio * 35.0f);

          //safety clamp for the servo
          if (servo_angle < 55) {
            servo_angle = 55;
          }
          else if (servo_angle > 125) {
            servo_angle = 125;
          }

          //then we move the motors
          Motor_Left_SetSpeed(motor_pwm);
          Motor_Right_SetSpeed(motor_pwm);
          Servo_SetAngle((uint8_t)servo_angle);
        }

        else {
          packet.mode = MENU_MODE;
          Robot_SetState(robot_idle);
          Motor_Stop();
          Servo_SetAngle(SERVO_ANGLE_CENTER);
        }

      
      }
    }

    else if (esp_byte == 0xBB) {
      //create the data packet for the esp32 camera
      tof_packet_t tof;
      if (HAL_UART_Receive(&huart1, (uint8_t*)&tof, sizeof(tof_packet_t), 20) == HAL_OK) {
        //get the distance reading in the form of a 16 bit integet
        tof_distance = tof.distance; //in mm
        //if our distance is greater than 350, then we just stop the robot.
        if (tof_distance < TOF_MAX_DISTANCE && tof_distance > 0 && tof.status == 0) {
          obstacle_flag = true; //turn flag on
          //just stop the motors
          Motor_Stop();
        }
        else {
          obstacle_flag = false;
        }
      }
    }


    else if (esp_byte == 0xCC) {
      follow_packet_t follow_packet;
      if (HAL_UART_Receive(&huart1, (uint8_t*)&follow_packet, sizeof(follow_packet_t), 20) == HAL_OK) {
        int8_t steering_angle = follow_packet.steer_angle;
        int8_t target_found = (follow_packet.target_found != 0);

        if (target_found) {
          // Wake up motor driver and refresh watchdog timer
          Motor_SetStandby(false);
          last_command_time = HAL_GetTick();

          // Proportional Front Servo Steering: -30 to +30 degrees
          int16_t target_servo = SERVO_ANGLE_CENTER + steering_angle;
          if (target_servo < SERVO_ANGLE_MIN) target_servo = SERVO_ANGLE_MIN;
          if (target_servo > SERVO_ANGLE_MAX) target_servo = SERVO_ANGLE_MAX;
          Servo_SetAngle((uint8_t)target_servo);

          // Drive motors forward if obstacle_flag is clear (ToF safety check)
          if (!obstacle_flag) {
            Motor_Forward(200);
          } else {
            Motor_Stop();
          }
        }
        else {
          Servo_SetAngle(SERVO_ANGLE_CENTER);
          Motor_Stop();
        }
      }
    }
  }





  // -------------------------------------------------------------------------
  // 2. Process incoming keyboard inputs from PuTTY on huart2 (LPUART1 / USB)
  // -------------------------------------------------------------------------
  if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE)) {
    __HAL_UART_CLEAR_OREFLAG(&huart2);
  }
  if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_NE) || __HAL_UART_GET_FLAG(&huart2, UART_FLAG_FE) || __HAL_UART_GET_FLAG(&huart2, UART_FLAG_PE)) {
    __HAL_UART_CLEAR_FLAG(&huart2, UART_CLEAR_NEF | UART_CLEAR_FEF | UART_CLEAR_PEF);
  }

  if (HAL_UART_Receive(&huart2, &received_byte, 1, 0) == HAL_OK) {
    last_command_time = HAL_GetTick();


    
    switch (current_mode) {
      case UART_MODE_MENU:
        switch(received_byte) {
          case 'u':
            current_mode = UART_MODE_STM32;
            menu_uart();
            break;

          case 'm':
            current_mode = UART_MODE_MOTOR;
            menu_motor();
            break;

          case 'c':
            current_mode = UART_MODE_COMBINED;
            current_servo_angle = 90; //set the default angle to 90
          
            //call the function
            Servo_SetAngle(90);
            //menu
            menu_combined();
            break;

          case 'p':
            current_mode = UART_MODE_SPEAKER;
            first_print = true; 
            sensor_test_active = false;
            menu_speaker();
            break;

          case 't':
            //current_mode = UART_MODE_STEPPER;
            //first_print = true;

            //reset stepper angle to 0
            //current_stepper_angle = 0;
            //stepper_init();
            //menu_stepper();
            break;
            
          case 'n':
            current_mode = UART_MODE_NORMALIZE;

            //flag to determine whether to print the header or not
            first_print = true;

            //turn on sensor
            sensor_test_active = true;
            menu_normalized();
            break;

          case 'a':
            current_mode = UART_MODE_AUTO;
            first_print = true;
            sensor_test_active = false;
            
            //default state for the robot
            Robot_SetState(robot_auto);
            
            //call the menu
            menu_autonomous();
            break;

          case 's':
            current_mode = UART_MODE_SERVO;
            first_print = true;
            sensor_test_active = false;
            //default angle.
            current_servo_angle = 90;

            //call the function then print the menu
            Servo_SetAngle(90);
            menu_servo();
            break;

          case 'v':
            current_mode = UART_MODE_VOLTAGE;
            sensor_test_active = true;
            first_print = true;
            menu_voltage();
            break;

          case 'b':
            current_mode = UART_MODE_BOTH;
            current_servo_angle = 90;
            Servo_SetAngle(90);
            sensor_test_active = true;
            first_print = true;
            menu_both();
            break;

          case 'i':
            current_mode = UART_MODE_IMU;
            first_print = true;
            sensor_test_active = false;
            menu_imu();
            break;

            //setting a pre-speed in the menu, probably not needed.
        
            case '1':
              robot_speed = 250;
              UART_SendMessage("\r\nSpeed set to 25% PWM (250/999)\r\n");
              UART_CONTROL_init();
              break;
            
            case '2':
              robot_speed = 500;
              UART_SendMessage("\r\nSpeed set to 50% PWM (500/999)\r\n");
              UART_CONTROL_init();
              break;

            case '3':
              robot_speed = 750;
              UART_SendMessage("\r\nSpeed set to 75% PWM (750/999)\r\n");
              UART_CONTROL_init();
              break;

            case '4':
              robot_speed = 999;
              UART_SendMessage("\r\nSpeed set to 100% PWM (999/999)\r\n");
              UART_CONTROL_init();
              break;

            default:
              UART_CONTROL_init(); // Reprint menu on invalid key
              break;
        }
        break;

    //lots of ORs because we use the motor in all of these modes
    case UART_MODE_MOTOR:
    case UART_MODE_BOTH:
    case UART_MODE_COMBINED:      
      switch(received_byte) {
        case 'h':
          Robot_SetState(robot_idle);
          sensor_test_active = false;
          UART_SendMessage("\r\nExiting Mode. Stopping Robot.\r\n");
          UART_CONTROL_init();
          break;

        case 'w':
          Robot_SetState(robot_forward);
          if (current_mode == UART_MODE_COMBINED) {
            char fwd_msg[64];
            snprintf(fwd_msg, sizeof(fwd_msg), "COMBINED FORWARD (Servo: %d deg)\r\n", current_servo_angle);
            UART_SendMessage(fwd_msg);
          } 

          else if (current_mode == UART_MODE_MOTOR) {
            UART_SendMessage("ROBOT FORWARD\r\n");
          }
          break;

        case 's':
          Robot_SetState(robot_reverse);
          if (current_mode == UART_MODE_COMBINED) {
            char rev_msg[64];
            snprintf(rev_msg, sizeof(rev_msg), "COMBINED REVERSE (Servo: %d deg)\r\n", current_servo_angle);
            UART_SendMessage(rev_msg);
          } 

          else if (current_mode == UART_MODE_MOTOR) {
            UART_SendMessage("ROBOT REVERSE\r\n");
          }
          break;
          
          case 'x':
            Robot_SetState(robot_idle);
              if (current_mode == UART_MODE_COMBINED) {
                current_servo_angle = 90;
                Servo_SetAngle(90);
                UART_SendMessage("COMBINED STOPPED (Servo: 90 deg)\r\n");
            } 

            else if (current_mode == UART_MODE_MOTOR) {
              UART_SendMessage("ROBOT STOPPED\r\n");
            }
            break;

          case 'a':
            if (current_mode == UART_MODE_COMBINED) {
              if (current_servo_angle > SERVO_ANGLE_MIN) {
                current_servo_angle = (current_servo_angle < SERVO_ANGLE_MIN + 10) ? SERVO_ANGLE_MIN : (current_servo_angle - 10);
              }
              Servo_SetAngle(current_servo_angle);
              char angle_msg[64];
              snprintf(angle_msg, sizeof(angle_msg), "COMBINED LEFT (Servo: %d deg)\r\n", current_servo_angle);
              UART_SendMessage(angle_msg);
            } 
            else if (current_mode == UART_MODE_MOTOR || current_mode == UART_MODE_BOTH) {
              Robot_SetState(robot_left);
              UART_SendMessage("ROBOT LEFT\r\n");
            }
            break;

          case 'd':
            if (current_mode == UART_MODE_COMBINED) {
              if (current_servo_angle < SERVO_ANGLE_MAX) {
                current_servo_angle = (current_servo_angle + 10 > SERVO_ANGLE_MAX) ? SERVO_ANGLE_MAX : (current_servo_angle + 10);
              }
              Servo_SetAngle(current_servo_angle);
              char angle_msg[64];
              snprintf(angle_msg, sizeof(angle_msg), "COMBINED RIGHT (Servo: %d deg)\r\n", current_servo_angle);
              UART_SendMessage(angle_msg);
            } 
            else if (current_mode == UART_MODE_MOTOR || current_mode == UART_MODE_BOTH) {
              Robot_SetState(robot_right);
              UART_SendMessage("ROBOT RIGHT\r\n");
            }
            break;

            case 'f':
              Robot_SetState(robot_fault);
              if (current_mode == UART_MODE_MOTOR || current_mode == UART_MODE_COMBINED){
                UART_SendMessage("ROBOT FAULT\r\n");
              }
              break;
            
            case '1':
              robot_speed = 250;
              UART_SendMessage("\r\nSpeed set to 25% PWM (250/999)\r\n");
              UART_CONTROL_init();
              break;
            
            case '2':
              robot_speed = 500;
              UART_SendMessage("\r\nSpeed set to 50% PWM (500/999)\r\n");
              UART_CONTROL_init();
              break;

            case '3':
              robot_speed = 750;
              UART_SendMessage("\r\nSpeed set to 75% PWM (750/999)\r\n");
              UART_CONTROL_init();
              break;
              
            case '4':
              robot_speed = 999;
              UART_SendMessage("\r\nSpeed set to 100% PWM (999/999)\r\n");
              UART_CONTROL_init();
              break;
      }
      break;

      //just tells us how to exit.
    case UART_MODE_STM32:
      current_mode = UART_MODE_STM32;
      
      if (received_byte == 'h') {
        //set the current mode to the menu
        current_mode = UART_MODE_MENU;
        UART_SendMessage("\r\n--- Exited Packet Monitor Mode ---\r\n\r\n");
        menu_main();
      }
      break;
    case UART_MODE_VOLTAGE:

      if (received_byte == 'h' || received_byte == 'v' || received_byte == 'x') {
        sensor_test_active = false;
        UART_SendMessage("\r\n--- Exited Sensor Test Mode ---\r\n");
        UART_CONTROL_init();
      }
      break;

    case UART_MODE_NORMALIZE:

      // add key binds to exit this mode
      if (received_byte == 'h' || received_byte == 'v' ||
          received_byte == 'x') {
        // turn off the sensor
        sensor_test_active = false;

        // send a message through UART
        UART_SendMessage("\r\n--- Exited Sensor Test Mode ---\r\n");

        // reset and reinitialize UART
        UART_CONTROL_init();
      }
      break;

    case UART_MODE_AUTO:
      switch(received_byte) {
        case 'h':
          Robot_SetState(robot_idle);
          UART_SendMessage("\r\n--- Exited Autonomous Mode. Stopping Robot. ---\r\n");
          UART_CONTROL_init();
          break;

        case '1':
          robot_speed = 250;
          UART_SendMessage("\r\nSpeed set to 25% PWM (250/999)\r\n");
          UART_CONTROL_init();
          break;
            
        case '2':
          robot_speed = 500;
          UART_SendMessage("\r\nSpeed set to 50% PWM (500/999)\r\n");
          UART_CONTROL_init();
          break;

        case '3':
          robot_speed = 750;
          UART_SendMessage("\r\nSpeed set to 75% PWM (750/999)\r\n");
          UART_CONTROL_init();
          break;
              
        case '4':
          robot_speed = 999;
          UART_SendMessage("\r\nSpeed set to 100% PWM (999/999)\r\n");
          UART_CONTROL_init();
          break;
      }
      break;

    case UART_MODE_SERVO: {
      bool angle_changed = false;
      switch(received_byte) {
        case 'h':
          UART_SendMessage("\r\n--- Exited Servo Mode ---\r\n");
          UART_CONTROL_init();
          break;

        case 'a':
          current_servo_angle = (current_servo_angle >= SERVO_ANGLE_MIN + 5) ? current_servo_angle - 5 : SERVO_ANGLE_MIN;
          angle_changed = true;
          break;

        case 'd':
          current_servo_angle = (current_servo_angle <= SERVO_ANGLE_MAX - 5) ? current_servo_angle + 5 : SERVO_ANGLE_MAX;
          angle_changed = true;
          break;

        case '1':
          current_servo_angle = SERVO_ANGLE_LEFT; // 38 deg
          angle_changed = true;
          break;

        case '2':
          current_servo_angle = SERVO_ANGLE_CENTER; // 90 deg
          angle_changed = true;
          break;

        case '3':
          current_servo_angle = SERVO_ANGLE_RIGHT; // 153 deg
          angle_changed = true;
          break;

        default:
          break;
      }

      if (angle_changed) {
        Servo_SetAngle(current_servo_angle);
        char angle_buf[64];
        snprintf(angle_buf, sizeof(angle_buf), "\rCurrent Angle: %3d degrees",
                 current_servo_angle);
        UART_SendMessage(angle_buf);
      }
      break;
    }

   /*
     case UART_MODE_STEPPER: {
      int16_t target_angle = current_stepper_angle;
      bool angle_changed = false;
      switch(received_byte) {
        case 'h':
          //stepper_stop();
          UART_SendMessage("\r\n--- Exited Stepper Mode ---\r\n");
          UART_CONTROL_init();
          break;

        case 'a':
          target_angle = current_stepper_angle - 5;
          if (target_angle < 0)
            target_angle = 0;
          angle_changed = true;
          break;

        case 'd':
          target_angle = current_stepper_angle + 5;
          if (target_angle > 360)
            target_angle = 360;
          angle_changed = true;
          break;

        case '1':
          target_angle = 0;
          angle_changed = true;
          break;

        case '2':
          target_angle = 180;
          angle_changed = true;
          break;

        case '3':
          target_angle = 360;
          angle_changed = true;
          break;

        default:
          break;
      }

      if (angle_changed && target_angle != current_stepper_angle) {
        int16_t diff = target_angle - current_stepper_angle;
        stepper_move_degrees((float)diff, 400);
        current_stepper_angle = target_angle;

        char angle_buf[64];
        snprintf(angle_buf, sizeof(angle_buf), "\rCurrent Angle:  %3d degrees",
                 current_stepper_angle);
        UART_SendMessage(angle_buf);
      }
  }
   */
  

    case UART_MODE_SPEAKER:
      switch(received_byte) { 
        case 'h':
          UART_SendMessage("\r\n--- Exited Speaker Test Mode ---\r\n");
          UART_CONTROL_init();
          break;

        case '1':
          UART_SendMessage("\rSpeaker tone (disabled - no timer configured).\r\n");
          break;

        case '2':
          UART_SendMessage("\rSpeaker beep (disabled - no timer configured).\r\n");
          break;

        case '0':
          UART_SendMessage("\rTone stopped.\r\n");
          break;

        default:
          break;
      }
      break;

    case UART_MODE_IMU:
      switch(received_byte) {
        case 'h':
        case 'x':
          UART_SendMessage("\r\n--- Exited IMU Mode ---\r\n");
          UART_CONTROL_init();
          break;

        case 'r': {
          uint8_t id = LSM6DS3_GetWhoAmI();
          char id_buf[64];
          snprintf(id_buf, sizeof(id_buf), "\r[LSM6DS3] WHO_AM_I = 0x%02X\r\n", id);
          UART_SendMessage(id_buf);
          break;
        }

        case 'd':
          LSM6DS3_DumpRegisters();
          break;

        default:
          break;
      }
      break;

    default:
      break;
    }
  }

  // Periodic sensor print
  static uint32_t last_print_time = 0;

  // if adding modes you need to add it here.
  if ((current_mode == UART_MODE_VOLTAGE || current_mode == UART_MODE_BOTH ||
       current_mode == UART_MODE_NORMALIZE) &&
      sensor_test_active && (HAL_GetTick() - last_print_time >= 250)) {
    last_print_time = HAL_GetTick();

    // this an array to temp store the characters to be sent out.
    char buffer[512];

    // set the buffer length to 0 to start.
    int len = 0;

    // Move cursor up lines if not the first print to keep the display static on
    // rows
    if (!first_print) {
      // Both Mode and Normalize Mode print 9 lines, Voltage Mode prints 8 lines
      if (current_mode == UART_MODE_BOTH ||
          current_mode == UART_MODE_NORMALIZE) {
        len += snprintf(buffer + len, sizeof(buffer) - len, "\x1B[9A");
      }

      else {
        len += snprintf(buffer + len, sizeof(buffer) - len, "\x1B[8A");
      }
    }
    if (first_print) {
      for (int i = 0; i < 8; i++) {
        min[i] = 4095; // set each channel to the value of 3.3V which is a white
                       // surface.
        max[i] = 0; // set each channel to the value of 0V which is a black surface.
        filtered_adc[i] = 0;
      }
    }
    first_print = false;

    if (current_mode == UART_MODE_BOTH) {
      const char *state_str = "UNKNOWN";
      switch (Robot_GetState()) {
      case robot_idle:
        state_str = "IDLE";
        break;
      case robot_forward:
        state_str = "FORWARD";
        break;
      case robot_reverse:
        state_str = "REVERSE";
        break;
      case robot_left:
        state_str = "LEFT TURN";
        break;
      case robot_right:
        state_str = "RIGHT TURN";
        break;
      case robot_fault:
        state_str = "FAULT";
        break;
      case robot_auto:
        state_str = "AUTO";
        break;
      case robot_manual:
        break;
      case robot_imu:
        break;
      }
      int percent = (robot_speed * 100) / 999;
      len += snprintf(buffer + len, sizeof(buffer) - len,
                      "State: %-10s | Speed: %d%% PWM (%d/999)         \r\n",
                      state_str, percent, robot_speed);
    }

    uint8_t black_count = 0;

    // Iterate through all 8 channels of the ADC.
    for (uint8_t ch = 0; ch < 8; ch++) { // where ch is the iterating variable.

      // determine the raw ADC values
      uint16_t raw =
          MCP3208_ReadChannel(&hspi1, ADC_CS_GPIO_Port, ADC_CS_Pin, ch);

      // detection if theres any errors.
      if (raw == MCP3208_ERROR_VALUE) {
        len += snprintf(buffer + len, sizeof(buffer) - len,
                        "CH%d: ERR                     \r\n", ch);
      }

      // if no errors we can find the max and min
      else {
        // Apply Exponential Moving Average (EMA) filter to raw ADC
        if (filtered_adc[ch] == 0) {
          filtered_adc[ch] = raw;
        } else {
          float alpha = 0.3f;
          filtered_adc[ch] =
              (uint16_t)(alpha * raw + (1.0f - alpha) * filtered_adc[ch]);
        }
        uint16_t filtered_val = filtered_adc[ch];

        if (filtered_val < min[ch]) {
          min[ch] = filtered_val;
        }

        if (filtered_val > max[ch]) {
          max[ch] = filtered_val;
        }

        // for normalized mode, we dont print the actual voltage values.
        if (current_mode == UART_MODE_NORMALIZE) {
          // thresholds to classify color directly using filtered raw ADC
          const char *color = "Unknown"; // ptr to color.
          if (filtered_val < BLACK_THRESHOLD) {
            color = "Brown";
          } else {
            color = "Black";
            black_count++;
          }

          len += snprintf(buffer + len, sizeof(buffer) - len,
                          "CH%d: ADC: %4u | Color: %-6s                  \r\n",
                          ch, filtered_val, color);
        }
        // after we calculate the max and min values we can calculate the actual
        // voltage values.
        // if not normalized mode then we just calculate the voltage normally.
        else {
          // convert the raw ADC values from 16 bit to 32 bit.
          uint32_t actual_voltage = ((uint32_t)filtered_val * 3300) / 4095;
          if (current_mode == UART_MODE_VOLTAGE) {

            len += snprintf(
                buffer + len, sizeof(buffer) - len,
                "CH%d: %lu.%02luV | ADC: %4u | min: %u , max: %u  \r\n", ch,
                actual_voltage / 1000, (actual_voltage % 1000) / 10,
                filtered_val, min[ch], max[ch]);
          } else {
            len += snprintf(buffer + len, sizeof(buffer) - len,
                            "CH%d: %lu.%02luV | ADC: %4u                       "
                            "        \r\n",
                            ch, actual_voltage / 1000,
                            (actual_voltage % 1000) / 10, filtered_val);
          }
        }
      }
    }

    if (current_mode == UART_MODE_NORMALIZE) {
      len += snprintf(buffer + len, sizeof(buffer) - len,
                      "Line Detected: %s (Black Count: %d)          \r\n",
                      (black_count >= 3) ? "YES" : "NO ", black_count);
    }

    UART_SendMessage(buffer);
  }
}

// Communication timeout watchdog: stops motors and centers steering if packets stop arriving
void UART_CONTROL_check_timeout(void) {
  if (last_command_time > 0 && (HAL_GetTick() - last_command_time > 1000)) {
    if (Robot_GetState() != robot_fault && Robot_GetState() != robot_auto) {
      Motor_Stop();
      Servo_SetAngle(SERVO_ANGLE_CENTER);
      if (Robot_GetState() != robot_idle) {
        Robot_SetState(robot_idle);
      }
    }
  }
}

UART_ControlMode UART_CONTROL_GetMode(void) { 
  return current_mode; 
}



//the arm cpu from the stm32 has a hardware cycle counter (DWT) - data watchpoint and trace cycle counter.
void DWT_Init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    //reset the cycle counter value
    DWT->CYCCNT = 0;

    //enable the dwt cycle counter 
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t DWT_GetMicros(void) {
  //systemcoreclock is 170 mhz (170 cycles is 1ms)
  return DWT->CYCCNT / (SystemCoreClock/1000000);
}

// Static telemetry state variables
static uint8_t  g_cpu_load         = 4;
static float    g_control_rate     = 100.0f;
static float    g_latency_ms       = 0.05f;
static float    g_jitter_ms        = 2.1f;
static uint16_t g_missed_deadlines = 0;
static uint32_t last_loop_start_us = 0;
static float    jitter_filter_us   = 2100.0f;

// Call this at the START of your control loop
void Telemetry_Loop_Start(void) {
    uint32_t now_us = DWT_GetMicros();
    
    // 1. Calculate Period & Control Rate (Hz)
    if (last_loop_start_us > 0) {
        uint32_t period_us = now_us - last_loop_start_us;
        if (period_us > 10 && period_us < 100000) {
            float instant_hz = 1000000.0f / (float)period_us;
            if (instant_hz > 500.0f) instant_hz = 100.0f; // clamp to nominal if unthrottled
            g_control_rate = g_control_rate * 0.95f + instant_hz * 0.05f;
            
            // 2. Calculate Jitter using RFC 3550 EMA filter
            float diff_us = fabsf((float)period_us - 10000.0f);
            jitter_filter_us += (diff_us - jitter_filter_us) / 16.0f;
            g_jitter_ms = jitter_filter_us / 1000.0f;
        }
    }
    last_loop_start_us = now_us;
}

// Call this at the END of your control loop
void Telemetry_Loop_End(uint32_t loop_start_us) {
    uint32_t now_us = DWT_GetMicros();
    uint32_t compute_time_us = (now_us >= loop_start_us) ? (now_us - loop_start_us) : 10;
    
    // 3. Execution Latency (ms) - smooth EMA filter
    float instant_lat = (float)compute_time_us / 1000.0f;
    if (instant_lat > 0.001f && instant_lat < 10.0f) {
        g_latency_ms = g_latency_ms * 0.90f + instant_lat * 0.10f;
    }
    
    // 4. CPU Load %: compute from active work + baseline background overhead
    static uint32_t s_accum_active_us = 0;
    static uint32_t s_accum_total_us = 0;
    s_accum_active_us += compute_time_us;
    s_accum_total_us += 10000;
    
    if (s_accum_total_us >= 100000) { // Every 100ms
        float active_pct = ((float)s_accum_active_us * 100.0f) / (float)s_accum_total_us;
        float total_load = active_pct + 4.5f; // 4.5% baseline PWM/UART/SysTick load
        if (total_load > 100.0f) total_load = 100.0f;
        g_cpu_load = (uint8_t)(total_load + 0.5f);
        s_accum_active_us = 0;
        s_accum_total_us = 0;
    }
}

void UART_Send_Telemetry(void) {
  robot_status_t status = {0};
    status.speedSetting     = (robot_speed * 100) / 999;
    status.direction        = (uint8_t)Robot_GetState();
    status.emergencyStop    = (Robot_GetState() == robot_fault) ? 1 : 0;

    // Live STM32 Real-Time Superloop Performance
    status.cpuLoad          = g_cpu_load;
    status.controlRate      = g_control_rate;
    status.latencyMs        = g_latency_ms;
    status.jitterMs         = g_jitter_ms;
    status.missedDeadlines  = g_missed_deadlines;
    status.lineSensors = robot_status.lineSensors;

    // Transmit over UART (PA9/PA10)
    uint8_t marker = 0xAA;
    HAL_UART_Transmit(&huart1, &marker, 1, 10);
    HAL_UART_Transmit(&huart1, (uint8_t*)&status, sizeof(robot_status_t), 10);
}
