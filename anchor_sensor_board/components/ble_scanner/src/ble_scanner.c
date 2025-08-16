#include "ble_scanner.h"
#include "esp_log.h"
#include "esp_gap_ble_api.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_ibeacon_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sntp_helper.h"
#include <string.h>

static const char *TAG = "BLE_ANCHOR";

// iBeacon 광고 데이터
static esp_ble_ibeacon_t ibeacon_adv_data;
static bool is_advertising = false;
static bool ble_initialized = false;

// 광고 파라미터 (전역으로 선언하여 재사용)
static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_SCAN_IND,
    // .adv_type = ADV_TYPE_ADV_SCAN_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// GAP 이벤트 콜백
static void gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    esp_err_t err;
    ESP_LOGI(TAG, "GAP event: %d", event); // 이벤트 번호 출력 (추가)
    
    switch (event) {
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                is_advertising = true;
                ESP_LOGI(TAG, "iBeacon advertising started (Interval: %d–%d ms)",
                         adv_params.adv_int_min * 625 / 1000,
                         adv_params.adv_int_max * 625 / 1000);
            } else {
                ESP_LOGE(TAG, "iBeacon advertising start failed: 0x%02x", param->adv_start_cmpl.status);
            }
            break;
            
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            is_advertising = false;
            ESP_LOGI(TAG, "iBeacon advertising stopped");
            break;

        default:
            break; // 불필요한 GAP 이벤트 무시
    }
}

// 주기적으로 광고 상태를 체크하는 태스크
static void anchor_status_task(void *pvParameters) {
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (1) {
        if (is_advertising) {
            ESP_LOGI(TAG, "iBeacon Anchor broadcasting - Major: %d, Minor: %d, RSSI@1m: %ddBm", 
                     ENDIAN_CHANGE_U16(vendor_config.major), 
                     ENDIAN_CHANGE_U16(vendor_config.minor),
                     (int8_t)vendor_config.measured_power);
        } else {
            ESP_LOGW(TAG, "iBeacon Anchor is NOT broadcasting");
        }
        
        // 정확한 10초 간격으로 실행
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(10000));
    }
}

// 광고 재시작 함수
esp_err_t ble_anchor_restart_advertising(void)
{
    if (!ble_initialized) {
        ESP_LOGE(TAG, "BLE not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (is_advertising) {
        esp_ble_gap_stop_advertising();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    ESP_LOGI(TAG, "Restarting iBeacon advertising...");

    esp_ble_config_ibeacon_data(&vendor_config, &ibeacon_adv_data);
    esp_err_t ret = esp_ble_gap_config_adv_data_raw((uint8_t*)&ibeacon_adv_data, sizeof(ibeacon_adv_data));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config raw adv data: %s", esp_err_to_name(ret));
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(200));

    ret = esp_ble_gap_start_advertising(&adv_params);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start advertising (manual): %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Advertising manually started");
    }

    return ret;
}



void ble_anchor_init(void)
{
    ESP_LOGI(TAG, "Initializing BLE Anchor (iBeacon Transmitter)");
    
    // 이미 초기화된 경우 스킵
    if (ble_initialized) {
        ESP_LOGW(TAG, "BLE Anchor already initialized");
        return;
    }
    
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_LOGI(TAG, "Registering GAP callback...");
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_cb));

    ESP_LOGI(TAG, "  - Major: %d (0x%04X)", ENDIAN_CHANGE_U16(vendor_config.major), vendor_config.major);
    ESP_LOGI(TAG, "  - Minor: %d (0x%04X)", ENDIAN_CHANGE_U16(vendor_config.minor), vendor_config.minor);
    ESP_LOGI(TAG, "  - Measured Power: %d dBm", (int8_t)vendor_config.measured_power);

    esp_ble_config_ibeacon_data(&vendor_config, &ibeacon_adv_data);
    esp_ble_gap_config_adv_data_raw((uint8_t*)&ibeacon_adv_data, sizeof(ibeacon_adv_data));

    ble_initialized = true;

    xTaskCreate(anchor_status_task, "anchor_status", 3072, NULL, 5, NULL);
    ESP_LOGI(TAG, "BLE Anchor initialized");
}

// 광고 상태 확인 함수
bool ble_anchor_is_advertising(void)
{
    return is_advertising;
}

// BLE 정리 함수
void ble_anchor_deinit(void)
{
    if (!ble_initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "Deinitializing BLE Anchor");
    
    if (is_advertising) {
        esp_ble_gap_stop_advertising();
    }
    
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    
    ble_initialized = false;
    is_advertising = false;
    
    ESP_LOGI(TAG, "BLE Anchor deinitialized");
}