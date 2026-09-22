#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
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
#include "imu.h"

#define failsafe_time 2000
bool failsafe_flag = false;

static data_packet_t last_sent_packet = {0};
uint32_t last_time_rx = 0;
uint32_t last_user_active_time = 0;
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


//we create the control task for the transmitter, this has a priority of 5, 
//the highest priority
static void control_task(void *pvParameters) {
    //we need to keep track of this so that we don't use blocking delays which makes the cpu do nothing for x time
    //instead we use curr_time - last_time > x_time 
    TickType_t curr_time = xTaskGetTickCount(); //this is the curr time of the cpu
    const TickType_t xFreq = pdMS_TO_TICKS(10); //this is how long the task should last.
    static uint32_t last_tx_time = 0; 
        

    while(1) {
        //we need to read the raw imu and gyrovalues
        //start the metrics loop so we can know metrics such as jitter, latency 
        metrics_record_loop_start(); 

        //we need to read the raw imu and gyrovalues
        
        // 1. Read raw IMU values
        int16_t gx = 0, gy = 0, gz = 0;
        int16_t ax = 0, ay = 0, az = 0;
        imu_read_raw(&gx, &gy, &gz, &ax, &ay, &az);

        // 2. Process tilt orientation and apply deadband filter
        int16_t tilt_x = 0, tilt_y = 0;
        imu_process_tilt(ax, ay, &tilt_x, &tilt_y);


        //prepare the data packet but clear it first
        data_packet_t packet = {0};
        packet.accel_x = tilt_x;
        packet.accel_y = tilt_y;
        packet.accel_z = az;
        packet.gyro_z = gz;

        //read and debounce physical buttons
        uint8_t raw_buttons = read_buttons();
        
        //fill the packet with the raw_buttons
        packet.button_data = raw_buttons; //lol forgot, using packet.button is for accessing struct var
        //packet->button is ptr like accessing ptr
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
        if (current_page == PAGE_MENU || current_page == PAGE_GITHUB || current_page == PAGE_LINKEDIN || current_page == PAGE_LEFTPAGE) {
            packet.button_data = 0; // Clear button mask so UI button presses don't move motors
            packet.joystick_x = 2000; // Force neutral center
            packet.joystick_y = 2000; // Force neutral center
            packet.speed = 0;
            packet.mode = MENU_MODE;
        } 
        else if (current_page == PAGE_AUTO) {
            extern bool auto_running;
            packet.button_data = 0;
            packet.joystick_x = 2000;
            packet.joystick_y = 2000;
            if (auto_running) {
                packet.mode = AUTO_MODE;
                packet.speed = current_speed;
            } 
            else {
                packet.mode = MENU_MODE; // Stay stationary when stopped
                packet.speed = 0;
            }
        } 
        else if (current_page == PAGE_MANUAL || current_page == PAGE_MANUAL_DATA) {
            packet.mode = MANUAL_MODE;
            packet.speed = current_speed;
        } 
        else if (current_page == PAGE_IMU) {
            extern bool imu_running;
            packet.button_data = 0;
            packet.joystick_x = 2000;
            packet.joystick_y = 2000;

            if (imu_running) {
                packet.mode = IMU_MODE;
                packet.speed = current_speed;
            } 
            else {
                packet.mode = MENU_MODE; // Stay stationary when stopped
                packet.speed = 0;
                packet.accel_x = 0;
                packet.accel_y = 0;
            }
        }

        // Always beep the speaker on real physical button presses (even in menu/auto)
        speaker_update(raw_buttons);


        //transmit every 25ms
        uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());
        if (now - last_tx_time >= 25) {
            last_sent_packet=packet;
            last_tx_time = now;
            metrics_record_espnow_tx_start();
            transmit_data(receiver_mac, &packet);
        }
        
        //failsafe check
        check_failsafe(&packet);

        metrics_record_loop_end();

        //preemptive determinisitc delay (every 10ms)
        vTaskDelayUntil(&curr_time, xFreq);

    }
}

//low priority of 1 or 2
static void console_task(void *pvParameters) {
    while(1) {
        UART_CONTROL_update();

        //print the joystick debug readings
        print_joystick_values();

        //run at 10Hz so it doesn't waste CPU cycles
        vTaskDelay(pdMS_TO_TICKS(100));
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
    init_imu();
    

    printf("\r\n==========================================\r\n");
    printf("   ESP32 TRANSMITTER READY               \r\n");
    printf("   FreeRTOS Preemptive Tasks Running     \r\n");
    printf("==========================================\r\n");

    //we pin the tasks to specific cores
    //contorl task on core0
    xTaskCreatePinnedToCore(control_task, "control_task", 4096, NULL, 5, NULL, 0);

    //console task core 0 as well.
    xTaskCreatePinnedToCore(console_task, "console_task", 2048, NULL, 1, NULL, 0);
    

}