#ifndef SERVO_H
#define SERVO_H

#include <main.h>
#include <stdbool.h>
#include <stdint.h>

// Steering Servo Angles (degrees)
#define SERVO_ANGLE_MIN    38
#define SERVO_ANGLE_MAX    153
#define SERVO_ANGLE_CENTER 90
#define SERVO_ANGLE_LEFT   38
#define SERVO_ANGLE_RIGHT  153

void servo_init(void);
void Servo_SetAngle(uint8_t angle);
void Servo_SetAngleImmediate(uint8_t angle);
void Servo_Update(void);
void Servo_SetSpeed(uint16_t step_interval_ms, float step_deg);
void Servo_SetAutoDetach(bool enable, uint16_t delay_ms);
void servo_stop(void);

uint8_t Servo_GetTargetAngle(void);
uint8_t Servo_GetCurrentAngle(void);
bool Servo_IsMoving(void);

#endif
