// beacon_scanner_task.h

#ifndef BEACON_SCANNER_TASK_H
#define BEACON_SCANNER_TASK_H

// mqtt 전송을 beacon에서 안 할거면 필요 없음
#include "mqtt_client.h"

// ble 설정
void ble_init(void);
void ble_scan_task(void *param);
esp_mqtt_client_handle_t mqtt_setup(void);  // mqtt 기능 분리 시 삭제

#endif