#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "driver/i2c_master.h"
#include "vl53l1x.h"
#include "tof.h"

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

    while(1) {
        //get the tof reading
        uint16_t dist = tof_get_distance();

        //then if the distance is not all 1111s like 0xFFFF
        if (dist != 0xFFFF) {
            ESP_LOGI(TAG, "Distance: %u mm ", dist);
        }
        vTaskDelay(pdMS_TO_TICKS(50)); //delay the cpu after by 50ms
    }


}