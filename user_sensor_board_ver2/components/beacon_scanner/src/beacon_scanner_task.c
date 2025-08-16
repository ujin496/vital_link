// beacon_scanner_task.c
// beacon scanner task는 mutex로 다루지 않음.

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "mqtt_client_wrapper.h" 

// NimBLE 헤더
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"

// sensor_data.h 추가
#include "sensor_data.h" 

static const char *TAG = "BEACON_SCANNER";

void ble_scan_task(void *param);

// BLE 동기화 후 스캔 시작
#include "freertos/event_groups.h"
static EventGroupHandle_t ble_event_group;
#define BLE_SYNC_DONE_BIT BIT0

// RSSI 신호 초기화
static int strongest_rssi = -999;
static uint16_t closest_major = 0;
static uint16_t closest_minor = 0;

// 필터링할 UUID - anchor의 UUID
static const uint8_t TARGET_UUID[16] = {
    0xFD, 0xA5, 0x06, 0x93,
    0xA4, 0xE2, 0x4F, 0xB1,
    0xAF, 0xCF, 0xC6, 0xEB,
    0x07, 0x64, 0x78, 0x25
};


// NimBLE task 함수
static void nimble_host_task(void *param) {
    nimble_port_run(); // NimBLE BLE host 실행
    nimble_port_freertos_deinit();
}

// BLE Sync 관련 함수
// 로그 출력용 함수 정의 필요 - ESP_LOGI 함수는 반환 값 없으므로 그냥 ESP_LOGI 함수를 인자로 박아 넣으면 안 됨.
static void ble_app_on_sync(void) {
    ESP_LOGI("BLE", "BLE Host synced");
    // BLE 이벤트 그룹 비트 세트
    xEventGroupSetBits(ble_event_group, BLE_SYNC_DONE_BIT);
    xTaskCreate(ble_scan_task, "ble_scan_task", 4096, NULL, 5, NULL);
}

// NimBLE 설정 초기화
void ble_init(void) {
    ble_event_group = xEventGroupCreate();  // synk 대기 용 BLE 이벤트 그룹 추가

    ESP_ERROR_CHECK(nimble_port_init());    // NimBLE 초기화
    ble_hs_cfg.sync_cb = ble_app_on_sync;  // 함수 등록
    nimble_port_freertos_init(nimble_host_task);    // NimBLE을 FreeRTOS 태스크로 실행
}

// BLE 광고 수신 콜백
static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    if (event->type != BLE_GAP_EVENT_DISC) return 0;

    const uint8_t *data = event->disc.data;
    uint8_t len = event->disc.length_data;
    int rssi = event->disc.rssi;

    // 데이터가 너무 짧으면 스킵
    if (len < 25) return 0;

    // iBeacon 형식 검증 - 올바른 위치에서 확인
    // BLE 광고 데이터 구조: [타입][길이][플래그][길이][Manufacturer Specific Data]
    // Manufacturer Specific Data: [타입][Company ID][iBeacon 데이터]
    if (len >= 26 && 
        data[0] == 0x02 && data[1] == 0x01 && data[2] == 0x06 && data[3] == 0x1a &&
        data[4] == 0xff && data[5] == 0x4c && data[6] == 0x00 &&
        data[7] == 0x02 && data[8] == 0x15) {
        
        // ESP_LOGI(TAG, "iBeacon format detected!");
        
        // UUID는 Manufacturer Specific Data 이후 9번째 바이트부터
        const uint8_t *uuid = &data[9];
        
        // UUID 출력 (디버깅용)
        // ESP_LOGI(TAG, "UUID: %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        //          uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
        //          uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);

        // UUID 필터링 - 지정된 anchor UUID만 허용
        if (memcmp(uuid, TARGET_UUID, 16) == 0) {
            // Major/Minor는 UUID 이후 16번째, 18번째 바이트
            uint16_t major = (data[25] << 8) | data[26];
            uint16_t minor = (data[27] << 8) | data[28];

            // ESP_LOGI(TAG, "Target iBeacon found: major=%d, minor=%d, rssi=%d", major, minor, rssi);

            // rssi 신호 비교하여 가장 가까운 anchor의 major/minor 값 출력
            if (rssi > strongest_rssi) {
                strongest_rssi = rssi;
                closest_major = major;
                closest_minor = minor;
                ESP_LOGI(TAG, "New strongest signal: major=%d, minor=%d, rssi=%d", 
                         closest_major, closest_minor, strongest_rssi);
            }
        } else {
            ESP_LOGI(TAG, "iBeacon found but UUID doesn't match target");
        }
    } else {
        // iBeacon이 아닌 다른 BLE 광고는 로그 제거 (선택사항)
        // ESP_LOGI(TAG, "Non-iBeacon packet received");
    }

    return 0;
}

// BLE 스캔 Task
void ble_scan_task(void *param) {
    struct ble_gap_disc_params scan_params = {
        .itvl = 0x30,    // 스캔 간격 줄임 (더 빈번하게)
        .window = 0x20,  // 스캔 윈도우 줄임
        .filter_policy = 0,
        .passive = 0,    // 액티브 스캔 유지
        .limited = 0
    };

    xEventGroupWaitBits(ble_event_group, BLE_SYNC_DONE_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    ESP_LOGI(TAG, "BLE scan task started");

    while (1) {
        strongest_rssi = -999;
        ESP_LOGI(TAG, "Starting BLE scan...");

        esp_err_t err = ble_gap_disc(0, BLE_HS_FOREVER, &scan_params, ble_gap_event, NULL);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "BLE scan failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // 스캔 시간을 늘려서 더 많은 패킷 수집
        vTaskDelay(pdMS_TO_TICKS(5000));  // 3초 → 5초로 증가
        
        ble_gap_disc_cancel();
        ESP_LOGI(TAG, "BLE scan stopped");

        if (strongest_rssi > -999) {
            sensor_data_set_location(closest_major, closest_minor, strongest_rssi);
            ESP_LOGI(TAG, "Location updated: major=%d, minor=%d, rssi=%d", 
                     closest_major, closest_minor, strongest_rssi);
        } else {
            ESP_LOGW(TAG, "No iBeacon found during scan");
        }

        vTaskDelay(pdMS_TO_TICKS(6000));
    }
}


