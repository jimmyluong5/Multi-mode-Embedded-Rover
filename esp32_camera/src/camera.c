#include "esp_rom_sys.h"
#include "camera.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include <esp_system.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// FireBeetle 2 ESP32-S3 DVP Camera Pinout
#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    45
#define CAM_PIN_SIOD     1
#define CAM_PIN_SIOC     2
#define CAM_PIN_D7      48
#define CAM_PIN_D6      46
#define CAM_PIN_D5       8
#define CAM_PIN_D4       7
#define CAM_PIN_D3       4
#define CAM_PIN_D2      41
#define CAM_PIN_D1      40
#define CAM_PIN_D0      39
#define CAM_PIN_VSYNC    6
#define CAM_PIN_HREF    42
#define CAM_PIN_PCLK     5

static camera_config_t camera_config = {
    .pin_pwdn     = CAM_PIN_PWDN,
    .pin_reset    = CAM_PIN_RESET,
    .pin_xclk     = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,

    .pin_d7       = CAM_PIN_D7,
    .pin_d6       = CAM_PIN_D6,
    .pin_d5       = CAM_PIN_D5,
    .pin_d4       = CAM_PIN_D4,
    .pin_d3       = CAM_PIN_D3,
    .pin_d2       = CAM_PIN_D2,
    .pin_d1       = CAM_PIN_D1,
    .pin_d0       = CAM_PIN_D0,
    .pin_vsync    = CAM_PIN_VSYNC,
    .pin_href     = CAM_PIN_HREF,
    .pin_pclk     = CAM_PIN_PCLK,

    .xclk_freq_hz = 20000000,
    .ledc_timer   = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG,
    .frame_size   = FRAMESIZE_QVGA,
    .jpeg_quality = 12,
    .fb_count     = 1,
    .fb_location  = CAMERA_FB_IN_DRAM, // Using internal SRAM (320KB available)
    .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
};

static esp_err_t last_init_error = ESP_FAIL;

void power_on_camera_pmic(void) {
    // Board is FireBeetle 2 ESP32-S3 V1.1+ with native hardware power supply
}

esp_err_t init_camera(void) {
    last_init_error = esp_camera_init(&camera_config);
    if (last_init_error != ESP_OK) {
        esp_rom_printf("[CAMERA] esp_camera_init failed: 0x%x (%s)\r\n", last_init_error, esp_err_to_name(last_init_error));
        return last_init_error;
    }
    sensor_t *s = esp_camera_sensor_get();
    esp_rom_printf("[CAMERA SUCCESS] OV3660 Ready! PID: 0x%04X\r\n", s ? s->id.PID : 0);
    return ESP_OK;
}

camera_fb_t* camera_take_picture(void) {
    if (last_init_error != ESP_OK) {
        static int retry_cnt = 0;
        if (++retry_cnt >= 6) {
            retry_cnt = 0;
            esp_rom_printf("[CAMERA] Retrying camera init...\r\n");
            init_camera();
        } else {
            esp_rom_printf("[CAMERA ERROR] Init failed (0x%x: %s)\r\n", last_init_error, esp_err_to_name(last_init_error));
        }
        return NULL;
    }
    camera_fb_t *pic = esp_camera_fb_get();
    if (!pic) {
        esp_rom_printf("[CAMERA ERROR] Frame capture failed!\r\n");
        return NULL;
    }
    esp_rom_printf("[CAMERA OK] OV3660 Frame Captured! Size: %u bytes (JPEG)\r\n", (unsigned int)pic->len);
    return pic;
}

void camera_return_picture(camera_fb_t* pic){
    if (pic) {
        esp_camera_fb_return(pic);
    }
}
