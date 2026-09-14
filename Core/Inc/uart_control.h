#ifndef UART_CONTROL_H
#define UART_CONTROL_H

#include <stdio.h>
#include <stdint.h>


#define MENU_MODE 0 
#define MANUAL_MODE 1
#define AUTO_MODE 2
#define IMU_MODE 3
#define TOTAL_MODES 4

typedef struct __attribute__((packed)) {
    uint8_t  button_data;
    uint8_t  speed;
    uint16_t joystick_x;
    uint16_t joystick_y;
    uint8_t  imu_x;
    uint8_t  imu_y;
    uint8_t  mode;
} data_packet_t;





typedef struct __attribute__((packed)) {
    float   actualspeed;
    uint8_t speedSetting;
    uint8_t direction;
    uint8_t lineSensors;
    uint8_t emergencyStop;
    uint8_t cpuLoad;
    float   controlRate;
    float   latencyMs;
    float   jitterMs;
    uint16_t missedDeadlines;
} robot_status_t;

//everytime you want to add a new mode, just add it here.
typedef enum {
  UART_MODE_MENU,
  UART_MODE_MOTOR,
  UART_MODE_COMBINED,
  UART_MODE_VOLTAGE,
  UART_MODE_BOTH,
  UART_MODE_NORMALIZE,
  UART_MODE_AUTO,
  UART_MODE_SERVO,
  UART_MODE_STEPPER,
  UART_MODE_SPEAKER,
  UART_MODE_IMU,
  UART_MODE_STM32
} UART_ControlMode;

void UART_CONTROL_init(void);
void UART_CONTROL_update(void);
void UART_CONTROL_check_timeout(void);
UART_ControlMode UART_CONTROL_GetMode(void);
void menu_imu(void);
void menu_main(void);
void menu_motor(void);
void menu_combined(void);
void menu_speaker(void);
void menu_stepper(void);
void menu_normalized(void);
void menu_autonomous(void);
void menu_servo(void);
void menu_voltage(void);
void menu_both(void);
void menu_uart(void);
void UART_Send_Telemetry(void);
void DWT_Init(void);
void Telemetry_Loop_Start(void);
void Telemetry_Loop_End(uint32_t loop_start_us);
uint32_t DWT_GetMicros(void);




#endif

