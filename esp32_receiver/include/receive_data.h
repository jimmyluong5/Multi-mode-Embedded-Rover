#ifndef RECEIVE_DATA_H 
#define RECEIVE_DATA_H 

#include <stdint.h>

typedef struct __attribute__((packed)) {     
    uint8_t button_data;
    uint8_t speed;
    //uint16_t sequence;
    uint16_t joystick_x;
    uint16_t joystick_y;
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_z;
    uint8_t mode; //eventually it'll contain more modes.
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


//this is from the esp32 camera
//we need to have the same data packet 
typedef struct __attribute__((packed)) {       
    uint8_t packet_type; //this will be 0xBB //distinguishes between transmitter packet
    //and the packet from esp32 camera
    uint16_t distance;
    uint8_t status;
}tof_packet_t;


typedef struct __attribute((packed)) {
    uint8_t packet_type;
    int8_t steer_angle;
    uint8_t target_found;
} follow_packet_t;


void receive_button_press(data_packet_t *packet);
void init_pins(void);
//void update_speed(data_packet_t* packet); not needed because we not updating speed at all 
//or calculating the speed of the car because the data packet sends us information.

void send_tof_stm32(tof_packet_t *packet);
void send_packet_stm32(data_packet_t *packet);
void send_follow_stm32(follow_packet_t *packet);
#endif