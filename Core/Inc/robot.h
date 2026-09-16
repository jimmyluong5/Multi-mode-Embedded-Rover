#ifndef ROBOT_H
#define ROBOT_H

#include <stdint.h>

typedef enum {
  robot_idle,
  robot_forward,
  robot_reverse,
  robot_left,
  robot_right,
  robot_fault,
  robot_imu,
  robot_auto, 
  robot_manual,
} RobotState;

extern volatile int16_t robot_speed;
void Robot_Init(void);
void Robot_SetState(RobotState new_state);
void Robot_Update(void);
RobotState Robot_GetState(void);
#endif
