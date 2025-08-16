// 예시 메인 코드
// app_main() -> FreeRTOS 진입점

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_system.h"

#include "wifi_connect.h"
#include "mqtt_client_wrapper.h"
#include "sensor_data.h"
#include "i2c_helper.h"
#include "sntp_helper.h"

#include "send_task.h"
#include "beacon_scanner_task.h"
#include "sensor_manager.h"

static const char *TAG = "MAIN";

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    esp_reset_reason_t reason = esp_reset_reason();
    printf("Reset reason: %d\n", reason);
    
    // NVS 초기화
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // I2C 초기화
    i2c_master_init();
    vTaskDelay(pdMS_TO_TICKS(500));

    // 센서 데이터 초기화
    sensor_data_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 센서 매니저 시작 (초기화 실패 시에도 계속 진행)
    ret = sensor_manager_start();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "센서 매니저 시작 실패, 계속 진행: %s", esp_err_to_name(ret));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Wi-Fi 연결
    wifi_connect();
    vTaskDelay(pdMS_TO_TICKS(2000));

    // MQTT 연결
    mqtt_start();

    // 블루투스 초기화
    ble_init();

    // MQTT 전송 태스크 시작
    start_send_task();
}