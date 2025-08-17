#include "mqtt_sender.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "mqtt_client_wrapper.h"
#include "esp_timer.h"
#include "sntp_helper.h"

extern esp_mqtt_client_handle_t mqtt_client;
extern bool mqtt_is_connected(void);  // 연결 상태 체크 함수

void mqtt_send_sensor_data(sensor_data_t data) {
    if (!mqtt_is_connected()) return;

    // SNTP 동기화 상태에 따라 적절한 타임스탬프 선택
    int64_t timestamp_to_send;
    const char* timestamp_type;
    
    if (is_sntp_synced()) {
        // SNTP 동기화된 경우: 유닉스 타임스탬프 사용
        time_t world_time = get_current_world_time();
        timestamp_to_send = (int64_t)world_time * 1000;  // 초를 밀리초로 변환
        timestamp_type = "unix_timestamp_ms";
    } else {
        // SNTP 동기화되지 않은 경우: ESP 타이머 사용
        timestamp_to_send = esp_timer_get_time() / 1000;  // 마이크로초를 밀리초로 변환
        timestamp_type = "esp_time_ms";
    }

    char payload[512]; // 위치 정보 포함으로 크기 증가
    snprintf(payload, sizeof(payload),
        "{\"measurement\": \"person\", \"tags\": {\"deviceId\": \"2\"}, "
        "\"fields\": {\"heartRate\": 76.6, \"temperature\": %.2f, \"spo2\": 97, \"steps\": %d, \"fallDetected\": %d}, "
        "\"location\": {\"major\": %d, \"minor\": %d, \"rssi\": %d}, "
        "\"time\": %" PRId64 "}",
        // data.heart_rate, 
        data.temperature, 
        // data.spo2, 
        data.steps, data.fall_detected, 
        data.location.major, 
        data.location.minor, data.location.rssi, 
        timestamp_to_send);

    esp_mqtt_client_publish(mqtt_client, "sensor/data", payload, 0, 1, 0);
    ESP_LOGI("MQTT_SEND", "Published: %s (timestamp: %lld, type: %s)", payload, timestamp_to_send, timestamp_type);
}
