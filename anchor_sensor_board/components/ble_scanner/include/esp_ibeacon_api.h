/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef __ESP_IBEACON_API_H__
#define __ESP_IBEACON_API_H__

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Major and Minor part are stored in big endian mode in iBeacon packet,
 * Need to use this macro to transfer while creating or processing iBeacon data.
 */
#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00)>>8) + (((x)&0xFF)<<8))

/* ESP_UUID:
 * If you create your own UUID, you can define it here.
 * This UUID should be unique to your application/organization
 */
#define ESP_UUID    {0xFD, 0xA5, 0x06, 0x93, 0xA4, 0xE2, 0x4F, 0xB1, 0xAF, 0xCF, 0xC6, 0xEB, 0x07, 0x64, 0x78, 0x25}

/* For iBeacon packet format, please refer to Apple "Proximity Beacon Specification" doc */

/* Constant part of iBeacon data */
typedef struct {
    uint8_t flags[3];       // BLE 기본 광고 플래그 ({0x02, 0x01, 0x06})
    uint8_t length;         // 광고 데이터 길이 (0x1A = 26 bytes)
    uint8_t type;           // 광고 타입 (0xFF = Manufacturer Specific Data)
    uint16_t company_id;    // Apple 회사 ID (0x004C)
    uint16_t beacon_type;   // iBeacon 타입 식별자 (0x1502)
} __attribute__((packed)) esp_ble_ibeacon_head_t;

/* Variable part of iBeacon data */
typedef struct {
    uint8_t proximity_uuid[16];  // 128-bit UUID (애플리케이션/조직 식별)
    uint16_t major;              // Major 번호 (큰 지역 구분, 0-65535)
    uint16_t minor;              // Minor 번호 (작은 지역/디바이스 구분, 0-65535) 
    int8_t measured_power;       // 1미터 거리에서의 수신 신호 강도 (RSSI dBm)
} __attribute__((packed)) esp_ble_ibeacon_vendor_t;

/* Complete iBeacon data structure */
typedef struct {
    esp_ble_ibeacon_head_t ibeacon_head;    // 고정 헤더 부분
    esp_ble_ibeacon_vendor_t vendor;        // 가변 데이터 부분
} __attribute__((packed)) esp_ble_ibeacon_t;

/* Default Major and Minor values */
#define MAJOR                0x0007
#define MINOR                0x0008

/* Global vendor configuration */
extern esp_ble_ibeacon_vendor_t vendor_config;

/**
 * @brief Configure iBeacon advertisement data
 * 
 * @param vendor_config  Pointer to vendor configuration data
 * @param ibeacon_adv_data  Pointer to output iBeacon data structure
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if parameters are NULL
 */
esp_err_t esp_ble_config_ibeacon_data(esp_ble_ibeacon_vendor_t *vendor_config, esp_ble_ibeacon_t *ibeacon_adv_data);

/**
 * @brief Update iBeacon configuration parameters
 * 
 * @param major  Major number (0-65535)
 * @param minor  Minor number (0-65535) 
 * @param measured_power  Signal strength at 1 meter in dBm (typically -59 to -30)
 * @return ESP_OK on success
 */
esp_err_t esp_ble_update_ibeacon_config(uint16_t major, uint16_t minor, int8_t measured_power);

#ifdef __cplusplus
}
#endif

#endif /* __ESP_IBEACON_API_H__ */