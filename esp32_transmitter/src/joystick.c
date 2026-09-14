#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "joystick.h"
#include "adc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdlib.h"

static const char *TAG = "JOYSTICK";

#define ADC_CENTER_X 2000 //average center for x
#define ADC_CENTER_Y 2000 //average center for y
#define ADC_DEADBAND 200 //absorbs noise
static volatile uint16_t s_latest_raw_x = ADC_CENTER_X;
static volatile uint16_t s_latest_raw_y = ADC_CENTER_Y;

//initialize the joysticks by calling the adc functions

void init_joystick(void){
    init_adc();
}

void get_joystick_screen_coords(int *out_x, int *out_y) {
    uint16_t raw_x = s_latest_raw_x;
    uint16_t raw_y = s_latest_raw_y;
    int dx = (int)raw_x - ADC_CENTER_X;
    int dy = (int)raw_y - ADC_CENTER_Y;
    // Deadband: If within +/- 200 of center, lock it dead-center!
    if (abs(dx) < ADC_DEADBAND) dx = 0;
    if (abs(dy) < ADC_DEADBAND) dy = 0;
    // Map deflection to screen pixels (using 1850 as max deflection)
    int pixel_x = GRID_CENTER_X + (dx * GRID_RADIUS_X) / 1850;
    int pixel_y = GRID_CENTER_Y - (dy * GRID_RADIUS_Y) / 1850;
    // Clamp inside the box boundary
    if (pixel_x < GRID_CENTER_X - GRID_RADIUS_X) pixel_x = GRID_CENTER_X - GRID_RADIUS_X;
    if (pixel_x > GRID_CENTER_X + GRID_RADIUS_X) pixel_x = GRID_CENTER_X + GRID_RADIUS_X;
    if (pixel_y < GRID_CENTER_Y - GRID_RADIUS_Y) pixel_y = GRID_CENTER_Y - GRID_RADIUS_Y;
    if (pixel_y > GRID_CENTER_Y + GRID_RADIUS_Y) pixel_y = GRID_CENTER_Y + GRID_RADIUS_Y;
    *out_x = pixel_x;
    *out_y = pixel_y;
}

void get_joystick_raw_values(uint16_t *out_x, uint16_t *out_y) {
    if (out_x) *out_x = s_latest_raw_x;
    if (out_y) *out_y = s_latest_raw_y;
}



uint16_t read_joystick_horizontal(void) {
    uint16_t value_x = read_joystick_x();
    uint16_t inv_x = 4095 - value_x;
    s_latest_raw_x = inv_x;
    return inv_x;
}

uint16_t read_joystick_vertical(void) {
    uint16_t value_y = read_joystick_y();
    s_latest_raw_y = value_y;
    return value_y;
}

void print_joystick_values(void) {

    static uint32_t last_print_time = 0; 
    uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount()); //current time in milliseconds

    if (now - last_print_time >= 400) { //has 200ms has passed?
        last_print_time = now; //remember when we last printed.

        uint16_t x = read_joystick_horizontal();
        uint16_t y = read_joystick_vertical();
        //ESP_LOGI(TAG, "Joystick X: %u | Y: %u", x, y);
    }
}

