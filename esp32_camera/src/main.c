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

void app_main() {
    esp_log_set_vprintf(esp_rom_vprintf);
    vTaskDelay(pdMS_TO_TICKS(1000)); // Allow USB-Serial to connect
    esp_rom_printf("\r\n\r\n========================================\r\n");
    esp_rom_printf("=== ESP32 FIREBEETLE 2 ONLINE (COM9) ===\r\n");
    esp_rom_printf("========================================\r\n\r\n");

    // Initialize Camera FIRST
    esp_rom_printf("[CAMERA] Initializing OV3660 Camera...\r\n");
    if (init_camera() == ESP_OK) {
        esp_rom_printf("[CAMERA SUCCESS] Camera Ready!\r\n");
        camera_fb_t* test_pic = camera_take_picture();
        if (test_pic) {
            camera_return_picture(test_pic);
        }
    } else {
        esp_rom_printf("[CAMERA FAILED] Could not initialize camera\r\n");
    }

    // Initialize Wi-Fi & ESP-NOW
    init_esp_nvs();
    init_wifi();
    init_esp_now();

    // Initialize ToF Sensor (non-fatal)
    esp_rom_printf("[TOF] Initializing ToF Sensor on D10/D11...\r\n");
    if (tof_init() == ESP_OK) {
        esp_rom_printf("[TOF SUCCESS] ToF Sensor Ready!\r\n");
    } else {
        esp_rom_printf("[TOF WARNING] ToF init failed, will stream default packets\r\n");
    }

    tof_packet_t packet;
    packet.packet_type = 0xBB;
    packet.status = 0;

    int counter = 0;
    while(1) {
        uint16_t dist = tof_get_distance();
        packet.distance = dist;
        packet.status = (dist == 0xFFFF) ? 1 : 0;
        send_tof_packet(&packet);

        // Every 500ms (~15 iterations), capture a frame and print status to PuTTY
        if (++counter >= 15) {
            counter = 0;
            camera_fb_t* pic = camera_take_picture();
            if (pic) {
                camera_return_picture(pic);
            }
            esp_rom_printf("[STREAM] ToF Distance: %u mm\r\n\r\n", dist);
        }

        vTaskDelay(pdMS_TO_TICKS(33)); // 30 Hz loop
    }
}
