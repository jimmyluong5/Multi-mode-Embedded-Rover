#ifndef TRANSMIT_DATA_H
#define TRANSMIT_DATA_H

#include <stdint.h>
#include "esp_now.h"
#include <stdbool.h>

#define MENU_MODE 0 
#define MANUAL_MODE 1
#define AUTO_MODE 2
#define IMU_MODE 3
#define TOTAL_MODES 4

extern uint8_t current_speed;


typedef struct __attribute__((packed)) {     
    uint8_t button_data;
    uint8_t speed;
    uint16_t joystick_x;
    uint16_t joystick_y;
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_z;
    uint8_t mode; //eventually it'll contain more modes.
} data_packet_t;

//change this to make it better to understand, make it like stm32 data packet.
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
extern robot_status_t robot_packet;
extern bool robot_packet_received;


//everytime you want to add a page, just add it here.
typedef enum {
    PAGE_MENU = 0,
    PAGE_MANUAL,
    PAGE_MANUAL_DATA, //2nd page of manual
    PAGE_AUTO,
    PAGE_AUTO_DATA,
    PAGE_IMU,
    PAGE_GITHUB,
    PAGE_LINKEDIN,
    PAGE_LEFTPAGE,
    PAGE_MAX_COUNT 
} page_t;

#define PAGE_EMPTY     PAGE_LEFTPAGE
#define PAGE_LEFT      PAGE_LEFTPAGE


void init_button_pin(void);
uint8_t read_buttons(void);
esp_err_t transmit_data(const uint8_t *receiver_mac, const data_packet_t *packet);
uint8_t update_speed(data_packet_t *packet);
uint8_t update_mode(data_packet_t *packet);

//static variables
extern page_t current_page; //this is the default mode
extern int hovered_mode; //which button is the cursor on
extern uint8_t active_mode; 
void process_arrow_keys(data_packet_t* packet);
void check_failsafe(data_packet_t* packet);

#endif /* TRANSMIT_DATA_H */