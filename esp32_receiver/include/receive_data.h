#ifndef RECEIVE_DATA_H 
#define RECEIVE_DATA_H 

#include <stdint.h>

typedef struct __attribute__((packed)) {  //the struct must be in proper order, its now 10 bytes, 9 bytes and 1 padded byte.   
    uint8_t button_data;
    uint8_t speed;
    //uint16_t sequence;
    uint16_t joystick_x;
    uint16_t joystick_y;
    uint8_t imu_x;        
    uint8_t imu_y;
    uint8_t mode;
} data_packet_t;


//this is from the stm32, now we send this to the other esp32
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

void receive_button_press(data_packet_t *packet);
void init_pins(void);
//void update_speed(data_packet_t* packet); not needed because we not updating speed at all 
//or calculating the speed of the car because the data packet sends us information.


void send_packet_stm32(data_packet_t *packet);
#endif