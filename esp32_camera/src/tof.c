#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "vl53l1x.h"

static const char *TAG = "example_ulp";

#define I2C_PORT_NUM 1
#define I2C_SCL_GPIO 14
#define I2C_SDA_GPIO 13
#define I2C_SPEED_HZ 400000
#define VL53L1X_ADDR_7BIT 0x29

static vl53l1x_t sensor = {0};

//returns error code or not

esp_err_t tof_init(void) {
    ESP_LOGI(TAG, "VL53L1X ULP example (long inter-measurement)");

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_PORT_NUM,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus = NULL;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));
    esp_err_t p = i2c_master_probe(bus, VL53L1X_ADDR_7BIT, pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "probe 0x%02X: %s", VL53L1X_ADDR_7BIT, esp_err_to_name(p));

    ESP_ERROR_CHECK(vl53l1x_init(&sensor, bus, VL53L1X_ADDR_7BIT));

    vTaskDelay(pdMS_TO_TICKS(100)); // minimaal
    // of 10–20 ms als je voeding/levelshifter traag is
    ESP_LOGI(TAG, "probe before init: %s", esp_err_to_name(i2c_master_probe(bus, 0x29, pdMS_TO_TICKS(100))));
    uint16_t id_pre = 0;
    ESP_LOGI(TAG, "id before init: %s, 0x%04X", esp_err_to_name(vl53l1x_get_sensor_id(&sensor, &id_pre)), id_pre);

    // ESP_ERROR_CHECK(vl53l1x_sensor_init(&sensor));
    esp_err_t err = vl53l1x_sensor_init(&sensor);
    uint16_t id = 0;
    ESP_LOGI(TAG, "id: %s 0x%04X", esp_err_to_name(vl53l1x_get_sensor_id(&sensor, &id)), id);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "vl53l1x_sensor_init failed: %s (0x%X)", esp_err_to_name(err), (unsigned)err);

        // Extra sanity check: lees SensorId om te zien of I2C nog werkt
        uint16_t id = 0;
        esp_err_t e2 = vl53l1x_get_sensor_id(&sensor, &id);
        ESP_LOGE(TAG, "get_sensor_id after init-fail: %s, id=0x%04X", esp_err_to_name(e2), id);

        return err;// stop, geen reboot-loop
    }

    // 
   
    ESP_ERROR_CHECK(vl53l1x_set_macro_timing(&sensor, 33));        // 33 ms integration window
    ESP_ERROR_CHECK(vl53l1x_set_intermeasurement_ms(&sensor, 33)); // 33 ms period (~30 Hz)


    ESP_ERROR_CHECK(vl53l1x_start(&sensor));
    return ESP_OK;
}


uint16_t tof_get_distance(void) {
    vl53l1x_result_t r = {0};

    esp_err_t err = vl53l1x_read(&sensor, &r, 100); //the timeout is 100ms, if it i2c gets stuck, 
    //stops waiting after 100ms
    if (err == ESP_OK && r.status == 0) {
        return r.distance_mm;
    }
    return 0xFFFF; //for invalid reading.

}
