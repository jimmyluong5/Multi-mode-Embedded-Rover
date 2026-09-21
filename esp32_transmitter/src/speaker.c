#include "speaker.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "stdbool.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define joystick_button GPIO_NUM_6
#define speaker_pin     GPIO_NUM_21

#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL    LEDC_CHANNEL_0
#define LEDC_DUTY_RES   LEDC_TIMER_10_BIT // 10-bit timer (0 to 1023 max)
#define LEDC_FREQUENCY  1024              // either 3072 or 1024 Hz
#define JOYSTICK_FREQ   2048 //or 2731 for loud
#define BUTTON_FREQ     1024

//static const char *TAG = "SPEAKER";

bool is_beeping = false;

void init_speaker(void) {

    //configure the active-low button
    gpio_reset_pin(joystick_button);
    gpio_set_direction(joystick_button, GPIO_MODE_INPUT);
    gpio_set_pull_mode(joystick_button, GPIO_PULLUP_ONLY);

    //configure the ledc timer for the speaker
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);

    //configure the ledc channel for gpio 21 which is the speaker.
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = speaker_pin,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&ledc_channel);

    //keep the pnp transistor off at startup
    ledc_stop(LEDC_MODE, LEDC_CHANNEL, 1);

}

void speaker_update(uint8_t button_packet) {
    // Non-blocking debounce for the joystick button
    static int joy_stable = 0;
    static int last_joy_raw = 0;
    static uint32_t last_joy_change = 0;

    int joy_raw = (gpio_get_level(joystick_button) == 0);
    uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());

    if (joy_raw != last_joy_raw) {
        last_joy_change = now;
        last_joy_raw = joy_raw;
    }

    if ((now - last_joy_change) >= 20) {
        joy_stable = joy_raw;
    }

    bool joystick_pressed = (joy_stable == 1);
    bool normal_button_pressed = (button_packet != 0);

    if (joystick_pressed) {
        //turn on the is beeping flag
        ledc_set_freq(LEDC_MODE, LEDC_TIMER, JOYSTICK_FREQ);
        speaker_on();
    }
    else if (normal_button_pressed) {
        ledc_set_freq(LEDC_MODE, LEDC_TIMER, BUTTON_FREQ);
        speaker_on();
    }
    
    else {
        //turn off the speaker and set the flag false.
            //stop the tone and the set output pin high to keep pnp transistor off.
        speaker_off();
    }
}

void speaker_on(void){
    if (!is_beeping) {
        is_beeping = true;
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 512);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
        //ESP_LOGI(TAG, "Speaker activated");
    }
}

void speaker_off(void){
    if (is_beeping) {
        ledc_stop(LEDC_MODE, LEDC_CHANNEL, 1);
        is_beeping = false; //set the flag off.
    }
}

void speaker_pattern(int beep_count, uint32_t beep_on, uint32_t beep_off) {
    for (int i = 0; i < beep_count; i++) {
        //as we do the beep count we just turn the speaker on and off, so turn on and have a delay 

        //so we set the frequency
        ledc_set_freq(LEDC_MODE, LEDC_TIMER, JOYSTICK_FREQ); //we can change the frequency its at 2048 Hz rn
        
        //then turn on the speaker
        speaker_on();
        //have a delay
        vTaskDelay(pdMS_TO_TICKS(beep_on));

        //then turn off
        speaker_off();
        vTaskDelay(pdMS_TO_TICKS(beep_off));
    }
    
}
