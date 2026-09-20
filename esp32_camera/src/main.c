#include "esp_rom_sys.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "vl53l1x.h"
#include "tof.h"
#include "wifi.h"
#include "camera.h"

static const char *TAG __attribute__((unused)) = "ESP32_Camera";

    void camera_task(void *pvParameters) {
        while(1) {
            camera_fb_t *pic = camera_take_picture();
            if (pic) {
                camera_return_picture(pic);
            }
            vTaskDelay(pdMS_TO_TICKS(5)); //~10
        }
    }


void app_main() {
    esp_log_set_vprintf(esp_rom_vprintf);
    //initialize the camera.
    init_camera();

    // Initialize Wi-Fi & ESP-NOW
    init_esp_nvs();
    init_wifi();
    init_esp_now();
    tof_init();


    xTaskCreatePinnedToCore(
        camera_task,
        "camera_task",
        8192, 
        NULL,
        5,  //PRIORITY OF (1-24), higher means hgiher priority.
        NULL, 
        1 //pin to core 1, and the other one will autonomically get core 
    );

    tof_packet_t packet;
    packet.packet_type = 0xBB;
    packet.status = 0;

    while(1) {
        uint16_t dist = tof_get_distance();
        packet.distance = dist;
        packet.status = (dist == 0xFFFF) ? 1 : 0;
        send_tof_packet(&packet);

        vTaskDelay(pdMS_TO_TICKS(33)); // 30 Hz loop
    }
}
