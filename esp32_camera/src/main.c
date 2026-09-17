#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "driver/i2c_master.h"
#include "vl53l1x.h"
#include "tof.h"
#include "wifi.h"

static const char *TAG = "ESP32_Camera";

void app_main() {
    ESP_LOGI(TAG, "Starting ESP32 Camera module");

    if (tof_init() == ESP_OK) {
        //just display a message
        ESP_LOGI(TAG, "ToF is ready");
    }
    else {
        ESP_LOGI(TAG, "FAILED");
    }

    init_esp_nvs();
    init_wifi();
    init_esp_now();

    //initialize the packet we finna send.
    tof_packet packet;
    packet.packet_type = 0xBB; 
    packet.status = 0;


    while(1) {
        //get the tof reading
        uint16_t dist = tof_get_distance();
        //then if the distance is not all 1111s like 0xFFFF
        if (dist != 0xFFFF) {
            ESP_LOGI(TAG, "Distance: %u mm ", dist);

            //update only the changing distance value and sned
            packet.distance = dist;
            send_tof_packet(&packet);
        }
        vTaskDelay(pdMS_TO_TICKS(33)); //delay the cpu after by 50ms
    }
}