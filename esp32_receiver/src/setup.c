#include "setup.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "hal/uart_types.h"
#include "nvs_flash.h"
#include "receive_data.h"
#include "driver/uart.h"
#include <string.h>

static const char *TAG = "RECEIVER_SETUP";

//this is the transmitter mac address.
uint8_t transmitter_mac[ESP_NOW_ETH_ALEN] = {0xAC, 0x27, 0x6E, 0xA1, 0x9F, 0x34};

bool g_transmitter_paired = false;

static void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
    
    //this is the transmitter data packet
    if (data_len == sizeof(data_packet_t)) { 
        data_packet_t packet;
        memcpy(&packet, data, sizeof(data_packet_t)); 
        receive_button_press(&packet);
    }
    else if (data_len == sizeof(tof_packet_t) && data[0] == 0xBB) {
        tof_packet_t tof;
        memcpy(&tof, data, sizeof(tof_packet_t)); 
        // Print distance straight to PuTTY
        ESP_LOGI("TOF_RECV", "ToF Distance: %u mm (Status: %d)", tof.distance, tof.status);
        //then we need to send the tof packet to the stm32.
        send_tof_stm32(&tof);
    }
    else if (data_len == sizeof(follow_packet_t) && data[0] == 0xCC) {
        follow_packet_t follow_packet;
        memcpy(&follow_packet, data, sizeof(follow_packet_t));
        ESP_LOGI("FOLLOW_RECV", "Follow Steer: %d, Found: %d", follow_packet.steer_angle, follow_packet.target_found);
        send_follow_stm32(&follow_packet);
    }


}

void init_wifi(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));
}

void init_esp_now(void) {
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(OnDataRecv));

    // Pre-register transmitter as peer
    esp_now_peer_info_t peer_info = {0};
    memcpy(peer_info.peer_addr, transmitter_mac, ESP_NOW_ETH_ALEN);
    peer_info.channel = 1;
    peer_info.encrypt = false;
    if (!esp_now_is_peer_exist(transmitter_mac)) {
        esp_now_add_peer(&peer_info);
    }
    g_transmitter_paired = true;
}

void init_esp_nvs(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void init_uart(void) {
    const int uart_buffer_size = (1024 * 2);

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, uart_buffer_size, uart_buffer_size, 0, NULL, 0));

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));

    // TX: GPIO 42 (Pin labeled "42") -> STM32 PA10 (RX / D0)
    // RX: GPIO 2  (Pin labeled "2")  <- STM32 PA9  (TX / D1)
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, 42, 2, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}
