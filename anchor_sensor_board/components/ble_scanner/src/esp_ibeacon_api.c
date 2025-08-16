#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "esp_ibeacon_api.h"
#include "esp_log.h"

static const char* IBEACON_DEMO_TAG = "IBEACON_DEMO";

/* For iBeacon packet format, please refer to Apple "Proximity Beacon Specification" doc */
/* Constant part of iBeacon data */
esp_ble_ibeacon_head_t ibeacon_common_head = {
    .flags = {0x02, 0x01, 0x06},
    .length = 0x1A,
    .type = 0xFF,
    .company_id = 0x004C,
    .beacon_type = 0x1502
};

/* Variable part of iBeacon data */
esp_ble_ibeacon_vendor_t vendor_config = {
    .proximity_uuid = ESP_UUID,
    .major = MAJOR,
    .minor = MINOR,
    .measured_power = 0xC5
};

// UUID 배열 정의 (헤더에서 extern으로 선언된 것)
uint8_t esp_uuid[16] = ESP_UUID;

esp_err_t esp_ble_config_ibeacon_data(esp_ble_ibeacon_vendor_t *vendor_config, esp_ble_ibeacon_t *ibeacon_adv_data)
{
    if ((vendor_config == NULL) || (ibeacon_adv_data == NULL)) {
        ESP_LOGE(IBEACON_DEMO_TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    // 구조체 초기화
    memset(ibeacon_adv_data, 0, sizeof(esp_ble_ibeacon_t));

    // 고정 헤더 복사
    memcpy(&ibeacon_adv_data->ibeacon_head, &ibeacon_common_head, sizeof(esp_ble_ibeacon_head_t));
    
    // 가변 데이터 복사
    memcpy(&ibeacon_adv_data->vendor, vendor_config, sizeof(esp_ble_ibeacon_vendor_t));

    // 데이터 검증 로그
    ESP_LOGI(IBEACON_DEMO_TAG, "iBeacon data configured - UUID: %02X%02X..., Major: 0x%04X, Minor: 0x%04X", 
             vendor_config->proximity_uuid[0], vendor_config->proximity_uuid[1],
             vendor_config->major, vendor_config->minor);

    return ESP_OK;
}