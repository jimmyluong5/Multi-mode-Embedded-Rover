#include "receive_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <esp_now.h>
#include <stdint.h>
#include <string.h>
#include <driver/gpio.h>
#include "esp_log.h"
#include "driver/uart.h"

#define LEFT_BTN 0
#define DOWN_BTN 1
#define UP_BTN 2
#define STOP_BTN 3
#define RIGHT_BTN 4

static const char *TAG = "ROVER_BRIDGE";

int led_pins[] = {
    GPIO_NUM_10, // 0 (left) - manual mode
    GPIO_NUM_11, // idx 1 (down) - decrease speed
    GPIO_NUM_12, // idx 2 (up) - increase speed
    GPIO_NUM_13, // idx 3 (Stop) - stop
    GPIO_NUM_14  // idx 4 (right) - autonomous mode
};

void init_pins() {
    for (int i = 0; i < 5; i++) {
        gpio_reset_pin(led_pins[i]);
        gpio_set_direction(led_pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(led_pins[i], 0);
    }
}

void receive_button_press(data_packet_t* packet) {
    ESP_LOGI(TAG, "[ESPNOW_RX] JoyX: %4u | JoyY: %4u | Speed: %3u | Mode: %u -> Forwarding to STM32 (Pin 42)", 
             packet->joystick_x, packet->joystick_y, packet->speed, packet->mode);

    // Forward packet directly over UART1 to STM32 PA3
    send_packet_stm32(packet);
    
    for (int i = 0; i < 5; i++) {   
        if ((packet->button_data & (1<<i)) != 0) {
            gpio_set_level(led_pins[i], 1);
        } else {
            gpio_set_level(led_pins[i], 0);
        }
    }
}

void send_packet_stm32(data_packet_t *packet) {
    uint8_t marker = 0xAA;
    uart_write_bytes(UART_NUM_1, (const char*)&marker, 1);
    uart_write_bytes(UART_NUM_1, (const char*)packet, sizeof(data_packet_t));
}

void send_tof_stm32(tof_packet_t *packet) {
    //need to pack the marker
    uint8_t marker = 0xBB;
    uart_write_bytes(UART_NUM_1, (const char*)&marker, 1);
    uart_write_bytes(UART_NUM_1, (const char*)packet, sizeof(tof_packet_t)); 

}