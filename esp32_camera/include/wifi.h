#ifndef WIFI_H
#define WIFI_H
#include <stdint.h>
#include "esp_now.h"
#include <stdbool.h>

typedef struct __attribute__((packed)) {       
    uint8_t packet_type; //this will be 0xBB //distinguishes between transmitter packet
    //and the packet from esp32 camera
    uint16_t distance;
    uint8_t status;

}tof_packet;

void init_esp_nvs(void);
void init_wifi(void);
void init_esp_now(void);
esp_err_t send_tof_packet(const tof_packet * packet);



#endif