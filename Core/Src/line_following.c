#include "MCP3208.h"
#include <line_following.h>
#include <main.h>
#include <stdio.h>
#include <stdlib.h>
#include "robot.h"
#include "motor.h"
#include "servo.h"
#include "uart_control.h"

#define CENTER_THRESHOLD 4
extern SPI_HandleTypeDef hspi1; // SPI1 for the MCP3208.

// defining the sensor max and min.
uint16_t sensor_min[8] = {0};
uint16_t sensor_max[8] = {4095};
extern robot_status_t robot_status;
#define BLACK_THRESHOLD 2359 // 1.90V on 3.3V ADC

uint16_t Robot_Normalize_ADC(uint16_t raw_val, uint16_t max_val, uint16_t min_val) {
  // clamp the inputs to the calibrated min/max range
  if (raw_val <= min_val) {
    return 0;
  }

  if (raw_val >= max_val) {
    return 1000;
  }

  // Calculate normalized value scaled to 0-1000
  return (uint16_t)(((uint32_t)(raw_val - min_val) * 1000U) / (max_val - min_val));
}

void Robot_Read_Normalized_Sensors(uint16_t *normalized_values) {
  for (uint8_t ch = 0; ch < 8; ch++) {
    //read the raw value from the external ADC via spi
    uint16_t raw = MCP3208_ReadChannel(&hspi1, ADC_CS_GPIO_Port, ADC_CS_Pin, ch);
    if (raw == MCP3208_ERROR_VALUE) {
      normalized_values[ch] = 0; //if we get an error then set the normalized values to 0.
    } 
    
    else {
      normalized_values[ch] = Robot_Normalize_ADC(raw, sensor_max[ch], sensor_min[ch]);
    }

  }
}

void Robot_LineFollow_Update(void) {
  // Rate-limit the control loop to 100Hz (every 10ms)
  static uint32_t last_loop_time = 0;
  uint32_t now = HAL_GetTick();

  if (now - last_loop_time < 10) {
    return;
  }
  last_loop_time = now;

  //array of voltage values.
  static uint16_t filtered_adc[8] = {0};
  static bool calibrated = false;
  static int32_t last_error = 0;

  // Initialize calibration limits on first run
  if (!calibrated) {
    for (int i = 0; i < 8; i++) {
      sensor_min[i] = 4095;
      sensor_max[i] = 0;
    }
    calibrated = true;
  }

  uint16_t sensors[8];

  // 1. Read and filter all 8 sensors
  for (uint8_t ch = 0; ch < 8; ch++) {
    //chip select allows the spi bus to listen to a specific device for data, so we choose to listen to the external ADC.
    uint16_t raw = MCP3208_ReadChannel(&hspi1, ADC_CS_GPIO_Port, ADC_CS_Pin, ch);
    if (raw == MCP3208_ERROR_VALUE) {
      sensors[ch] = 0;
    } 
    else {
      // Exponential Moving Average (EMA) noise filter
      if (filtered_adc[ch] == 0) {
        filtered_adc[ch] = raw;
      } 
      else {
        float alpha = 0.3f;
        filtered_adc[ch] = (uint16_t)(alpha * raw + (1.0f - alpha) * filtered_adc[ch]);
      }
      sensors[ch] = filtered_adc[ch];

      // Update calibration bounds dynamically
      //okay just updating the minimum and maximum of the channels.
      if (sensors[ch] < sensor_min[ch]) {
        sensor_min[ch] = sensors[ch];
      }
      if (sensors[ch] > sensor_max[ch]) {
        sensor_max[ch] = sensors[ch];
      }
    }
  }

  //we need to add this to the telemetry data packet
  robot_status.lineSensors = 0;
  for (uint8_t ch = 0; ch<8; ch++) {
    if (sensors[ch] >= BLACK_THRESHOLD) {
      robot_status.lineSensors |= (1<<ch); //set the bits for the specific channel. 
    }
  }


  if (Robot_GetState() != robot_auto) {
    return;
  }


  // 2. Identify consecutive black sensors to find the line position
  int8_t best_start = -1; 
  int8_t longest_len = 0; //this is the longest streak of black line channels that lined up we found
  
  int8_t current_start = -1; //initialize to sentinel value.
  int8_t current_len = 0; //this tracks the streak of channels that had consecutive.

  //we need to keep track of the previous or the last channel we saw
  //static variable because if the function ends, we need to know the previous value
  static int prev_channel[8] = {0}; 
  static int curr_channel[8] = {0};
  //each bit will correspond to the channel.

  for (int8_t i = 0; i < 8; i++) {

    //if the sensors greater than the black threshold then we can keep track of how many consecutive black channels we got

    if(sensors[i] >= BLACK_THRESHOLD) {
      //this will always update 
      //then we need to assign that index and set it high for the channel and
      curr_channel[i] = 1; //sets the corresponding index if one of those channels meet the black threshold.

      if (current_start == -1) {
        current_start = i;
      }
      
      current_len++; //also increase the length of those consecutive black channels.
    } 
    else { //if we are not greater than the black threshold, that means we don't see a black channel then,
      //we need to calculate the best/max number of consecutive black channels and its length. 
      
      //if the channels don't see the black line we must also set the current channel to 0, 
      //since its a static variable, it will remember the old 1s, this will ensure that each function call,
      //we get the most recent channel readings.
      curr_channel[i] = 0;
      if (current_len > longest_len) {
        best_start = current_start;
        longest_len = current_len;
      }
      current_start = -1; //then reset the counter back.
      current_len = 0;
    }
  }

  //updating the maxes.
  if (current_len > longest_len) {
    best_start = current_start;
    longest_len = current_len;
  }

  //after we have updated the maxes, then we check if its greater than 0.

    //we need to update the previous channel to the current channel after we have toggled through all the channels 
  //then we need to update the prev channel

  //but we need to check if we actually got a successful sequence of black channels
  if (longest_len > 0 && longest_len < 6 ) {
      //then we can actually populate the previous channel with the current channel
      for (int i = 0; i<8; i++) {
        prev_channel[i] = curr_channel[i];
      }
  }
   


  // Handle line recovery if no black sensors are found (longest_len == 0)

  //if this longest_len is 0, then we have lost the line right.
  if (longest_len == 0) {
    //we need to look at the previous channels and follow the lines
    //if any of the channels on the left are set high then we move left and vice versa for the right
    for (int i = 0; i < 8; i++) {
      //because i defined 
      // 0 1 2 3 4 5 6 7 

      // L L L C C R R R
      if (i<4 && prev_channel[i] == 1) {
        //then we must move the the motors to the right to find the line again
        Motor_Right_SetSpeed(150);
        Motor_Left_SetSpeed(150);
        Servo_SetAngle(SERVO_ANGLE_LEFT);
        break;
      }
      if (i>=4 && prev_channel[i] == 1) {
        //then the line was on the right
        Motor_Right_SetSpeed(-150);
        Motor_Left_SetSpeed(150);
        Servo_SetAngle(SERVO_ANGLE_LEFT);
        break;
      }
    }
    return; //just exits the function, code after this won't run, but its in a if statement so its fine.
  }

   // Static timer to drive straight through cross-junctions
  static int16_t intersection_cooldown = 0;
  int16_t fwd_speed = (robot_speed < 150) ? 150 : robot_speed;
  // If in cooldown, keep driving straight through without checking PID:
  if (intersection_cooldown > 0) {
    intersection_cooldown--;
    Servo_SetAngle(SERVO_ANGLE_CENTER);
    Motor_Left_SetSpeed(fwd_speed);
    Motor_Right_SetSpeed(fwd_speed);
    last_error = 0;
    return;
  }

  // Trigger Intersection Pass-Through when 5 or more sensors are black:
  if (longest_len >= 5) {
    intersection_cooldown = 12; // Lock straight for ~100ms
    Servo_SetAngle(SERVO_ANGLE_CENTER);
    Motor_Left_SetSpeed(fwd_speed);
    Motor_Right_SetSpeed(fwd_speed);
    last_error = 0;
    return;
  }


  

  int32_t error = 0;
  int8_t best_end = best_start + longest_len - 1;

  // Check if we are centered: at least 3 black in a row centered on the middle (CH2, CH3, CH4 or CH3, CH4, CH5)
  bool centered = (longest_len >= CENTER_THRESHOLD && best_start >= 2 && best_end <= 5);
  //CENTER_THRESHOLD = 3, because the robot is in the center or the line is centered,
  //as long as 3 consecutive channels are black.

  //if true, then our error is 0 obviously.
  if (centered) {
    error = 0;
  } 

  //then we are not centered.
  else {
    // Calculate center position of the consecutive black run (range 0 to 7000)
    int32_t position = (int32_t)((best_start + best_end) * 500);
    error = position - 3500;
  }

  // 3. PID Control Calculation
  float Kp = 0.35f;  // Proportional gain
  float Ki = 0.0f; // Integral gain (adjust as needed, start small) 
  float Kd = 0.5f;   // Derivative gain

//original was 
//0.3
//0.001
//0.8
  
  static int32_t integral = 0;

  // Accumulate the error over time (integral)
  integral += error;

  // Anti-windup protection: clamp the integral to prevents massive overshoot
  if (integral > 10000) {
    integral = 10000;
  }
  if (integral < -10000) {
    integral = -10000;
  }

  // Clear the accumulated integral when centered to prevent over-correcting
  //makes sense, if our robot is centered clear the integral term that measures our error from the center of the line.
  if (centered) {
    integral = 0;
  }

  int32_t p_term = error;
  int32_t i_term = integral;
  int32_t d_term = error - last_error;
  last_error = error;

  int32_t adjustment = (int32_t)(Kp * p_term + Ki * i_term + Kd * d_term);

  // 4. Drive Motors
  int16_t base_speed = robot_speed / 2;
  if (base_speed < 150) {
    base_speed = 150; // Minimum driving speed
  }

  
  //calculate how sharp the turn is (0 - straight, 3500 = extreme corner)

  int32_t turn_severity = abs(error);

  //slow down base speed by up to 50% on sharp curves.
  int16_t dynamic_speed = base_speed - (int16_t)((turn_severity*base_speed) /7000);


  int16_t left_motor_speed = dynamic_speed + adjustment;
  int16_t right_motor_speed = dynamic_speed - adjustment;

  if (turn_severity > 2000) {
    if (error>0) {
      //sharp left, we gotta slow down and reverse left wheel to pivot sharply
      left_motor_speed = -50;
    }
    else {
      right_motor_speed = -50; //for sharp right turns
    }
  }


  // Clamp speeds to safe bounds
  if (left_motor_speed > robot_speed) {
    left_motor_speed = robot_speed;
  }
  if (left_motor_speed < -robot_speed)  {
    left_motor_speed = -robot_speed;
  }

  if (right_motor_speed > robot_speed)  {
    right_motor_speed = robot_speed;
  }
  if (right_motor_speed < -robot_speed)  {
    right_motor_speed = -robot_speed;
  }
  Motor_Left_SetSpeed(left_motor_speed);
  Motor_Right_SetSpeed(right_motor_speed);

    // 5. Anticipation Steering (Reacts INSTANTLY the moment a curve begins)
  int32_t error_rate = error - last_error;
  int32_t steer_input = error + (error_rate * 3); // Leads into the turn immediately
  int16_t target_servo_angle = SERVO_ANGLE_CENTER + (int16_t)((steer_input * 45) / 1350);
  if (target_servo_angle < SERVO_ANGLE_MIN) {
    target_servo_angle = SERVO_ANGLE_MIN;
  }
  if (target_servo_angle > SERVO_ANGLE_MAX) {
    target_servo_angle = SERVO_ANGLE_MAX;
  }
  Servo_SetAngle((uint8_t)target_servo_angle);
}
