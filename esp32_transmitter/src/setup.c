#include "setup.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/idf_additions.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "ESP32_TRANSMITTER";

// Broadcast address: sends to any nearby receiver

//this should be the mac of the 2nd esp32 (receiver).
//so basically we need to find it's mac address and put it here.
//Media Access Control (MAC) - address is a unique 12-digit 
//code used to identify a device on a network.
uint8_t receiver_mac[ESP_NOW_ETH_ALEN] = {0xAC, 0x27, 0x6E, 0xA2, 0x87, 0x5C};

#include "metrics.h"
#include "transmit_data.h"

extern uint32_t last_time_rx; //extern means the variable actually exists from another .c file.
robot_status_t robot_packet = {0};
bool robot_packet_received = false;

//function to send data
static void OnDataSent(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
  if (tx_info == NULL)  {
    return;
  }
  //if our status is good then we stop the timer. 
  metrics_record_espnow_tx_done(status);
}


//function to receive data
static void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
  //if we got the length of the packet back from the stm32
  if (data_len == sizeof(robot_status_t)) {
    memcpy(&robot_packet, data, sizeof(robot_status_t));
    robot_packet_received = true;
    //update the last time we received the packet
    last_time_rx = pdTICKS_TO_MS(xTaskGetTickCount());

    //record incoming rssi and packet count
    int8_t rssi = (esp_now_info && esp_now_info->rx_ctrl) ? esp_now_info->rx_ctrl->rssi : 0;
    metrics_record_espnow_rx(rssi);

    ESP_LOGI(TAG, "Telemetery RX OK! CPU: %u%%, Latency: %.1fms, RSSI: %d dBm, Sensors: 0x%02X",
             robot_packet.cpuLoad, robot_packet.latencyMs, rssi, robot_packet.lineSensors);
  } else {
    ESP_LOGW(TAG, "Telemetry size mismatch! Got %d bytes, expected %u bytes",
             data_len, (unsigned int)sizeof(robot_status_t));
  }
}

void init_esp_nvs(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
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

  uint8_t transmitter_mac[6];
  esp_read_mac(transmitter_mac, ESP_MAC_WIFI_STA);
  ESP_LOGI(TAG, "************************************************");
  ESP_LOGI(TAG, "TRANSMITTER MAC: %02X:%02X:%02X:%02X:%02X:%02X",
           transmitter_mac[0], transmitter_mac[1], transmitter_mac[2],
           transmitter_mac[3], transmitter_mac[4], transmitter_mac[5]);
  ESP_LOGI(TAG, "************************************************");
}

void init_esp_now(void) {
  ESP_ERROR_CHECK(esp_now_init()); //this returns an esp_err_t, which is 
  //an error code. we use this to check if the function was successful.
  ESP_ERROR_CHECK(esp_now_register_send_cb(OnDataSent)); //this also returns an esp_err_t.
  ESP_ERROR_CHECK(esp_now_register_recv_cb(OnDataRecv)); //register incoming telemetry callback

  esp_now_peer_info_t peerInfo = {}; //initialize the peer info struct.
  //it's just a struct that holds the information of the peer.

  //this basically copies the data from the receiver_mac array to the 
  //peerInfo.peer_addr array.
  memcpy(peerInfo.peer_addr, receiver_mac, ESP_NOW_ETH_ALEN);
  //we set the channel to 1 because that's what we set the wifi channel to.
  peerInfo.channel = 1;
  //we set encrypt to false because we're not encrypting the data.
  peerInfo.encrypt = false;

  //we check if the peer exists, if it does not, we add it.
  if (!esp_now_is_peer_exist(receiver_mac)) {
    //this also returns an esp_err_t.
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      ESP_LOGE(TAG, "Failed to add peer"); //if it fails to add peer, it'll print this. 
    }
  }
}



