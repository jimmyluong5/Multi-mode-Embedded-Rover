#include "robot.h"
#include "main.h"
#include "motor.h"
#include "line_following.h"
#include "servo.h"

// Default motor speed / PWM duty cycle (0 to 999)
// 0 = 0% PWM, 500 = 50% PWM, 999 = 100% PWM
#define MOTOR_DEFAULT_SPEED 999

static RobotState current_state = robot_idle;
volatile int16_t robot_speed = MOTOR_DEFAULT_SPEED;

void Robot_Init(void) {
  current_state = robot_idle;
  Motor_Init();
  HAL_GPIO_WritePin(LED2_GPIO_PORT, LED2_PIN, GPIO_PIN_RESET);
}

void Robot_SetState(RobotState new_state) {
  current_state = new_state;
  if (current_state == robot_idle || current_state == robot_fault) {
    HAL_GPIO_WritePin(LED2_GPIO_PORT, LED2_PIN, GPIO_PIN_RESET);
  } 
  else {
    HAL_GPIO_WritePin(LED2_GPIO_PORT, LED2_PIN, GPIO_PIN_SET);
  }

  // Update motor speeds and steering servo according to the new robot state
  switch (current_state) {
  case robot_forward:
    Motor_Forward(robot_speed);
    break;
  case robot_reverse:
    Motor_Reverse(robot_speed);
    break;
  case robot_left:
    // Spin turn left: left motor backward, right motor forward
    Motor_Left_SetSpeed(-robot_speed);
    Motor_Right_SetSpeed(robot_speed);
    break;
  case robot_right:
    // Spin turn right: left motor forward, right motor backward
    Motor_Left_SetSpeed(robot_speed);
    Motor_Right_SetSpeed(-robot_speed);
    break;
  case robot_manual:
    break; //we do nothing because the movement is controlled by the joystick
  case robot_idle:
    Motor_Stop();
    Servo_SetAngle(SERVO_ANGLE_CENTER);
    break;
  case robot_fault:
    Motor_Brake();
    Servo_SetAngle(SERVO_ANGLE_CENTER);
    break;
  case robot_auto:
    Motor_Stop();
    Servo_SetAngle(SERVO_ANGLE_CENTER);
    break;
  default:
    Motor_Stop();
    Servo_SetAngle(SERVO_ANGLE_CENTER);
    break;
  }
}

void Robot_Update(void) {
  if (current_state == robot_auto) {
    Robot_LineFollow_Update();
  }
}

RobotState Robot_GetState(void) { 
  return current_state; 
}
