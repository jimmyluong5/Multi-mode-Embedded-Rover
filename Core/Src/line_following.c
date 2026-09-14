#include "MCP3208.h"
#include <line_following.h>
#include <main.h>
#include <stdio.h>
#include <stdlib.h>
#include "robot.h"
#include "motor.h"
#include "servo.h"

#define CENTER_THRESHOLD 4
extern SPI_HandleTypeDef hspi1; // SPI1 for the MCP3208.

// defining the sensor max and min.
uint16_t sensor_min[8] = {0};
uint16_t sensor_max[8] = {4095};

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
      if (sensors[ch] < sensor_min[ch]) {
        sensor_min[ch] = sensors[ch];
      }
      if (sensors[ch] > sensor_max[ch]) {
        sensor_max[ch] = sensors[ch];
      }
    }
  }



  // 2. Identify consecutive black sensors to find the line position
  int8_t best_start = -1;
  int8_t best_len = 0;
  
  int8_t current_start = -1;
  int8_t current_len = 0;

  for (int8_t i = 0; i < 8; i++) {
    if (sensors[i] >= BLACK_THRESHOLD) {
      if (current_start == -1) {
        current_start = i;
      }
      current_len++;
    } else {
      if (current_len > best_len) {
        best_start = current_start;
        best_len = current_len;
      }
      current_start = -1;
      current_len = 0;
    }
  }
  if (current_len > best_len) {
    best_start = current_start;
    best_len = current_len;
  }

  // Handle line recovery if no black sensors are found (best_len == 0)
  if (best_len == 0) {
    if (last_error < 0) {
      // Line was to the left, spin left in place
      Motor_Left_SetSpeed(-150);
      Motor_Right_SetSpeed(150);
      Servo_SetAngle(SERVO_ANGLE_LEFT);
    } else {
      // Line was to the right, spin right in place
      Motor_Left_SetSpeed(150);
      Motor_Right_SetSpeed(-150);
      Servo_SetAngle(SERVO_ANGLE_RIGHT);
    }
    return;
  }

  int32_t error = 0;
  int8_t best_end = best_start + best_len - 1;

  // Check if we are centered: at least 3 black in a row centered on the middle (CH2, CH3, CH4 or CH3, CH4, CH5)
  bool centered = (best_len >= CENTER_THRESHOLD && best_start >= 2 && best_end <= 5);

  if (centered) {
    error = 0;
  } 
  else {
    // Calculate center position of the consecutive black run (range 0 to 7000)
    int32_t position = (int32_t)((best_start + best_end) * 500);
    error = position - 3500;
  }

  // 3. PID Control Calculation
  float Kp = 0.15f;  // Proportional gain
  float Ki = 0.001f; // Integral gain (adjust as needed, start small)
  float Kd = 0.8f;   // Derivative gain

  static int32_t integral = 0;

  // Accumulate the error over time (integral)
  integral += error;

  // Anti-windup protection: clamp the integral to prevent massive overshoot
  if (integral > 10000) {
    integral = 10000;
  }
  if (integral < -10000) {
    integral = -10000;
  }

  // Clear the accumulated integral when centered to prevent over-correcting
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

  int16_t left_motor_speed = base_speed + adjustment;
  int16_t right_motor_speed = base_speed - adjustment;

  // Clamp speeds to safe bounds
  if (left_motor_speed > robot_speed) left_motor_speed = robot_speed;
  if (left_motor_speed < -robot_speed) left_motor_speed = -robot_speed;

  if (right_motor_speed > robot_speed) right_motor_speed = robot_speed;
  if (right_motor_speed < -robot_speed) right_motor_speed = -robot_speed;

  Motor_Left_SetSpeed(left_motor_speed);
  Motor_Right_SetSpeed(right_motor_speed);

  // 5. Update Servo Steering Angle (Dynamic Proportional Steering)
  int16_t target_servo_angle = SERVO_ANGLE_CENTER + (int16_t)((error * 45) / 3500);
  if (target_servo_angle < SERVO_ANGLE_MIN) target_servo_angle = SERVO_ANGLE_MIN;
  if (target_servo_angle > SERVO_ANGLE_MAX) target_servo_angle = SERVO_ANGLE_MAX;
  Servo_SetAngle((uint8_t)target_servo_angle);
}
