#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "mqtt_client.h"

#include "wifi_connect.h"
#include "mqtt_client_wrapper.h"
#include "sensor_data.h"
#include "i2c_helper.h"
#include "ble_scanner.h"

#include "send_task.h"
#include "tvoc_sensor.h"
#include "temp_humid_sensor.h"
#include "light_sensor.h"
#include "sntp_helper.h"


#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_ibeacon_api.h"


static const char *TAG = "MAIN";
extern esp_ble_ibeacon_vendor_t vendor_config;


void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());

    vendor_config.major = ENDIAN_CHANGE_U16(2);   // 원하는 major 값
    vendor_config.minor = ENDIAN_CHANGE_U16(1);   // 원하는 minor 값

    ESP_LOGI(TAG, "BLE iBeacon advertising 시작 시도...");
    ble_anchor_init();
    vTaskDelay(pdMS_TO_TICKS(1000));


    while (!ble_anchor_is_advertising()) {
        ESP_LOGW(TAG, "⚠ BLE iBeacon advertising 실패 - 재시도 중...");
        ble_anchor_restart_advertising();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    ESP_LOGI(TAG, "✓ BLE iBeacon advertising 성공 - 시스템 초기화 진행");

    
    // Wi-Fi 연결
    wifi_connect();
    
    // Wi-Fi 연결 완료까지 대기
    vTaskDelay(pdMS_TO_TICKS(3000));

    // SNTP 초기화 및 시간 동기화 추가
    ESP_LOGI(TAG, "SNTP 시간 동기화 시작...");
    esp_err_t sntp_result = sntp_init_and_sync();
    if (sntp_result == ESP_OK) {
        ESP_LOGI(TAG, "✓ SNTP 시간 동기화 성공");
    } else {
        ESP_LOGW(TAG, "⚠ SNTP 시간 동기화 실패, ESP 타이머 사용");
    }
    
    // MQTT 시작
    mqtt_start();
    
    // MQTT 연결 완료까지 대기
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // I2C 드라이버 초기화
    i2c_master_init();
    
    // 센서 데이터 구조체 초기화
    sensor_data_init();

    // 센서 초기화
    tvoc_sensor_init();  // 공기질 센서 초기화 및 태스크 시작
    temp_humid_sensor_init();  // 온습도 센서 초기화 및 태스크 시작
    //  GL5549(조도) 초기화 (GPIO32 = ADC1_CH4)
    ESP_ERROR_CHECK(light_sensor_init(ADC1_CHANNEL_4));



    // 센서 초기화 완료까지 대기
    vTaskDelay(pdMS_TO_TICKS(1000));

    // MQTT 전송 태스크 시작
    start_send_task();
    
    ESP_LOGI(TAG, "모든 초기화 완료");
}
