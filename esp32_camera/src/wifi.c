#include "wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "hal/uart_types.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include <string.h>

static const char *TAG = "ESP32 - Camera";
uint8_t receiver_mac[ESP_NOW_ETH_ALEN] = {0xAC, 0x27, 0x6E, 0xA2, 0x87, 0x5C};

//initialize the tof packet
tof_packet_t packet = {0};

//flag if we sent the packet correctly
//bool sent_packet = false; //maybe later

bool esp32_camera_to_receiver = false; //flag to see if we are paired.

//need the function to accept data for future uses
static void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
    if (esp32_camera_to_receiver && esp_now_info != NULL) {
        //check if the other end exists
        if (!esp_now_is_peer_exist(receiver_mac)) {
            esp_now_peer_info_t peer_info ={0};
            memcpy(peer_info.peer_addr, receiver_mac, 6);
            peer_info.channel = 1;
            peer_info.encrypt = false;

            if (esp_now_add_peer(&peer_info) == ESP_OK) {
                esp32_camera_to_receiver= true;
                ESP_LOGI(TAG, "Paired with Receiver MAC: " MACSTR, MAC2STR(receiver_mac));
            }
         
           
        }

        else {
            esp32_camera_to_receiver = true;
        }
    }
}

//function to send the data if we want to receive data
/* 
static void OnDataSent(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
  if (tx_info == NULL)  {
    return;
  }
}

 */

void init_esp_nvs(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
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
    memcpy(peer_info.peer_addr, receiver_mac, ESP_NOW_ETH_ALEN);
    peer_info.channel = 1;
    peer_info.encrypt = false;
    if (!esp_now_is_peer_exist(receiver_mac)) {
        esp_now_add_peer(&peer_info);
    }
}

esp_err_t send_tof_packet(tof_packet_t *packet) { 
    return esp_now_send(receiver_mac, (const uint8_t*)packet, sizeof(tof_packet_t));
}
