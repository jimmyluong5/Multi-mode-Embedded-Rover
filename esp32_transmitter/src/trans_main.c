#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "setup.h"
#include "transmit_data.h"
#include "uart_control.h"
#include <stdio.h>
#include "adc.h"
#include "joystick.h"
#include "speaker.h"
#include "stdint.h"
#include <string.h>
#include "esp_log.h"
#include "math.h"
#include "lcd.h"
#include "metrics.h"
#include "stdbool.h"
#include "esp_mac.h"

#define failsafe_time 2000
bool failsafe_flag = false;

static data_packet_t last_sent_packet = {0};
uint32_t last_time_rx = 0;
void deadband_filter(data_packet_t* packet, uint16_t raw_x, uint16_t raw_y) {
 // Deadband filter
        if (abs((int)raw_x - (int)last_sent_packet.joystick_x) < 25) {
            packet->joystick_x = last_sent_packet.joystick_x;
        } 
        else {
            packet->joystick_x = raw_x;
        }
        if (abs((int)raw_y - (int)last_sent_packet.joystick_y) < 25) {
            packet->joystick_y = last_sent_packet.joystick_y;
        } 
        else {
            packet->joystick_y = raw_y;
        }
}

uint32_t last_user_active_time = 0;

void check_failsafe(data_packet_t *packet) {
    // If we're in the manual page
    if (current_page == PAGE_MANUAL || current_page == PAGE_MANUAL_DATA) {
        uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());
        
        if (last_user_active_time == 0) {
            last_user_active_time = now;
        }

        // Check if we pressed a button or the joystick moved
        int16_t jx = metrics_get_joy_x_val();
        int16_t jy = metrics_get_joy_y_val();
        bool user_active = (packet->button_data != 0 || abs(jx) > 10 || abs(jy) > 10); 
        
        if (user_active) {
            last_user_active_time = now; // Reset the inactivity timer on user input
        }

        // If 5 seconds of idle inactivity passed, trip!
        if ((now - last_user_active_time) > 5000) {
            failsafe_flag = true; 
            last_user_active_time = now;
            speaker_pattern(8, 75, 75);
            ESP_LOGW("FAILSAFE", "Inactivity timeout (5s), activating failsafe!");
            // Return to menu
            current_page = PAGE_MENU;
            active_mode = MENU_MODE;
            hovered_mode = MANUAL_MODE;
        }
    } else {
        last_user_active_time = 0;
    }
}
void app_main(void) {
    // 1. Peripherals, NVS, WiFi, LCD, UART & ESP-NOW initialization
    init_esp_nvs();
    init_wifi();
    init_esp_now();
    init_button_pin();
    init_joystick();
    init_speaker();
    init_lcd_driver();
    UART_CONTROL_init();
    

    printf("\r\n==========================================\r\n");
    printf("   ESP32 TRANSMITTER READY               \r\n");
    printf("   Pure ESP-NOW Button Transmission Ready \r\n");
    printf("==========================================\r\n");

    static uint32_t last_time = 0;
    //static uint8_t current_speed = 0;

    //eventually we will get rid of this super loop with preemptive scheduling 
    while (1)
    {
        metrics_record_loop_start(); 


        // Check for serial console commands
        UART_CONTROL_update();


        // Read joystick and buttons

        //create a clean zeroed out packet strcuture for this 10ms time frame.
        data_packet_t packet = {0};

        //print joystick debug readings to the console, not technically needed.
        print_joystick_values();


        //read the buttons first, and we sample the 5 push buttons with the debounce algo 
        packet.button_data = read_buttons();

        //call the new arrow and mode processor, then from reading the buttons we know how to process 
        //the arrow keys
        process_arrow_keys(&packet);

        // Read the analog ADC voltages from the joystick
        uint16_t raw_x = read_joystick_horizontal();
        uint16_t raw_y = read_joystick_vertical();

        // Deadband filter for joystick readings
        deadband_filter(&packet, raw_x, raw_y);

        // MODE-SPECIFIC SAFETY ISOLATION:
        // Prevent menu navigation buttons and idle joystick drift from driving the rover
        if (current_page == PAGE_MENU || current_page == PAGE_GITHUB || 
            current_page == PAGE_LINKEDIN || current_page == PAGE_LEFTPAGE) {
            packet.button_data = 0; // Clear button mask so UI button presses don't move motors
            packet.joystick_x = 2000; // Force neutral center
            packet.joystick_y = 2000; // Force neutral center
            packet.speed = 0;
            packet.mode = MENU_MODE;
        } else if (current_page == PAGE_AUTO || current_page == PAGE_AUTO_DATA) {
            extern bool auto_running;
            packet.button_data = 0;
            packet.joystick_x = 2000;
            packet.joystick_y = 2000;
            if (auto_running) {
                packet.mode = AUTO_MODE;
                packet.speed = current_speed;
            } else {
                packet.mode = MENU_MODE; // Stay stationary when stopped
                packet.speed = 0;
            }
        } else if (current_page == PAGE_MANUAL || current_page == PAGE_MANUAL_DATA) {
            packet.mode = MANUAL_MODE;
            packet.speed = current_speed;
        } else if (current_page == PAGE_IMU || current_page == PAGE_IMU_DATA) {
            packet.mode = IMU_MODE;
            packet.speed = current_speed;
        }

        speaker_update(packet.button_data);

        // Transmit at a steady 40 Hz (every 25ms)
        uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());
        if (now - last_time >= 25) {
            last_sent_packet = packet;
            last_time = now;
            
            // Start the transmission time
            metrics_record_espnow_tx_start();
            transmit_data(receiver_mac, &packet);
        }
        //check failsafe every iteration
        check_failsafe(&packet);

        
       
        
        metrics_record_loop_end();

        //freertos non blocking delay, puts this task to sleep for 10ms, 
        //lets the cpu tackle other tasks 
        vTaskDelay(pdMS_TO_TICKS(10));  
    }
}