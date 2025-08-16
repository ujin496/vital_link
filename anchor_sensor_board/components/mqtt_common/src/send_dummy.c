// 더미 데이터 전송용 데모 코드 (사용하지 않음 - send_task.c에서 처리)

/*
#include "send_dummy.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "mqtt_client.h"
#include "mqtt_client_wrapper.h"
#include "temp_humid_sensor.h"
#include "tvoc_sensor.h"
#include <stdio.h>

static const char *TAG = "MQTT_SENDER";

// 외부에서 초기화된 MQTT 핸들을 받아야 합니다 (전역 또는 인자로 전달)
extern esp_mqtt_client_handle_t mqtt_client;

void send_sensor_data()
{
    if (mqtt_client == NULL || mqtt_is_connected() == false) {
        ESP_LOGW(TAG, "MQTT not connected. Skipping publish.");
        return;
    }

    // 실제 센서 데이터 읽기
    float temperature, humidity;
    if (!read_temp_humid_data(&temperature, &humidity)) {
        ESP_LOGW(TAG, "온습도 센서 읽기 실패");
        temperature = -999.0f;
        humidity = -999.0f;
    }
    
    // TVOC 데이터 읽기
    float rs = mq135_get_rs();
    float ratio = 0.0f;
    float tvoc = 0.0f;
    
    if (rs > 0) {
        ratio = mq135_get_ratio(rs);
        tvoc = mq135_get_tvoc_ppb(ratio);
    } else {
        ESP_LOGW(TAG, "TVOC 센서 읽기 실패");
        rs = -1.0f;
        ratio = -1.0f;
        tvoc = -1.0f;
    }

    int64_t timestamp = esp_timer_get_time() / 1000;  // ms 단위

    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"measurement\": \"sensor_data\", \"tags\": {\"deviceId\": \"dev01\"}, "
             "\"fields\": {\"temperature\": %.2f, \"humidity\": %.2f, \"tvoc\": %.2f, "
             "\"rs\": %.2f, \"ratio\": %.2f}, "
             "\"time\": %" PRId64 "}",
             temperature, humidity, tvoc, rs, ratio, timestamp);

    int msg_id = esp_mqtt_client_publish(mqtt_client, "sensor/data", payload, 0, 1, 0);
    ESP_LOGI(TAG, "Published sensor data with msg_id=%d: %s", msg_id, payload);
}

void sensor_data_task(void *pvParameters)
{
    while (1) {
        send_sensor_data();
        vTaskDelay(pdMS_TO_TICKS(10000));  // 10초 대기
    }
}
*/