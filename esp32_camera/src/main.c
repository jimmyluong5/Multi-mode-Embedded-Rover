#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "driver/i2c_master.h"
#include "vl53l1x.h"
#include "tof.h"
#include "wifi.h"
#include "camera.h"
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
    

    /*
    if (init_camera() == ESP_OK) {
        ESP_LOGI(TAG, "Camera Ready!");
        //test picture capture
        camera_fb_t* pic = camera_take_picture();
        if (pic) {
            camera_return_picture(pic);
        }
    }
    else {
        ESP_LOGE(TAG, "Camera failed - continuing with ToF sensor only");
    }
    */

    //initialize the packet we finna send.
    tof_packet_t packet;
    packet.packet_type = 0xBB; 
    packet.status = 0;


    while(1) {
        //get the tof reading
        uint16_t dist = tof_get_distance();
        //then if the distance is not all 1111s like 0xFFFF
        
        //load the distance into the packet
        packet.distance = dist;
        packet.status = (dist== 0xFFFF) ? 1:0;
        send_tof_packet(&packet);
        if (dist != 0xFFFF) {
            ESP_LOGI(TAG, "Distance: %u mm ", dist);
        }
        vTaskDelay(pdMS_TO_TICKS(33)); //delay the cpu after by 50ms
    }
}