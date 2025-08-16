#include "send_task.h"
#include "sensor_data.h"
#include "mqtt_sender.h"
#include "tvoc_sensor.h"
#include "temp_humid_sensor.h"
#include "light_sensor.h"
#include "sntp_helper.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "SEND_TASK";

// 실제로 주기적으로 실행되는 태스크 함수
void sensor_publish_task(void *pvParameters)
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
        ESP_LOGD(TAG, "timestamp set (%s): %lld", timestamp_type, (long long)timestamp);

        // 온습도 데이터 저장 (getters 사용)
        float temperature = get_temperature();
        float humidity    = get_humidity();
        sensor_data_set_temperature(temperature);
        sensor_data_set_humidity(humidity);

        // TVOC 데이터 읽기
        float rs = mq135_get_rs();
        if (rs > 0) {
            float ratio = mq135_get_ratio(rs);
            float tvoc = mq135_get_tvoc_ppb(ratio);

            sensor_data_set_rs(rs);
            sensor_data_set_ratio(ratio);
            sensor_data_set_tvoc(tvoc);
        } else {
            ESP_LOGW(TAG, "TVOC 센서 읽기 실패");
            sensor_data_set_rs(-1.0f);
            sensor_data_set_ratio(-1.0f);
            sensor_data_set_tvoc(-1.0f);
        }

        // 조도 데이터 읽기
        float lux;
        if (light_sensor_read_lux(&lux) == ESP_OK) {
            sensor_data_set_lux(lux);
        } else {
            ESP_LOGW(TAG, "조도 센서 읽기 실패");
            sensor_data_set_lux(-1.0f);
        }

        // 구조체 복사 (mutex로 보호됨)
        sensor_data_t snapshot = sensor_data_get_snapshot();

        // 새로운 InfluxDB 형식으로만 전송
        influx_sensor_data_t influx_data;
        sensor_data_convert_to_influx(&snapshot, &influx_data, "dev01");
        mqtt_send_influx_sensor_data(&influx_data);

        // 다음 전송까지 대기 (5초)
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// app_main에서 호출할 시작 함수
void start_send_task(void)
{
    xTaskCreate(sensor_publish_task, "sensor_publish_task", 4096, NULL, 5, NULL);
}
