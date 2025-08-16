#include "send_task.h"
#include "sensor_data.h"
#include "mqtt_sender.h"
#include "sntp_helper.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "SEND_TASK";

// 실제로 주기적으로 실행되는 태스크 함수
void send_task(void *pvParameters)
{
    while (1) {
        // SNTP 동기화 상태에 따라 타임스탬프 설정
        int64_t timestamp;
        const char* timestamp_type;
        
        if (is_sntp_synced()) {
            // SNTP 동기화된 경우: 유닉스 타임스탬프 사용
            time_t world_time = get_current_world_time();
            timestamp = (int64_t)world_time * 1000;  // 초를 밀리초로 변환
            timestamp_type = "unix_timestamp_ms";
        } else {
            // SNTP 동기화되지 않은 경우: ESP 타이머 사용
            timestamp = esp_timer_get_time() / 1000;  // 마이크로초를 밀리초로 변환
            timestamp_type = "esp_time_ms";
        }
        
        sensor_data_set_timestamp(timestamp);

        // 구조체 복사 (mutex로 보호됨)
        sensor_data_t snapshot = sensor_data_get_snapshot();
        
        // 유효한 측정값이 있는지 확인
        if (sensor_data_has_valid_measurements()) {
            int valid_count = sensor_data_get_valid_count();
            ESP_LOGI(TAG, "Sending data with %d valid sensors, timestamp: %lld (%s, SNTP synced: %s)", 
                     valid_count, timestamp, timestamp_type, is_sntp_synced() ? "YES" : "NO");
            
            // MQTT 전송 - mqtt_sender.c 내부 함수
            mqtt_send_sensor_data(snapshot);
        } else {
            ESP_LOGW(TAG, "Skipping MQTT send - no valid measurements");
        }

        // 다음 전송까지 대기 (1초)
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// app_main에서 호출할 시작 함수
void start_send_task(void)
{
    xTaskCreate(send_task, "send_task", 4096, NULL, 5, NULL);
}
