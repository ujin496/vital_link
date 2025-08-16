#include "temp_humid_sensor.h"
#include <stdio.h>
#include "dht.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

#define DHT_GPIO_PIN    GPIO_NUM_4     // DHT 센서 연결 핀
#define DHT_SENSOR_TYPE DHT_TYPE_DHT11 // DHT11 또는 DHT_TYPE_DHT22

/*
GPIO 4 - 맨 왼쪽S(Signal)
가운데 (VIN)
오른쪽 - (GND)
*/

static const char *TAG = "TEMP_HUMID_SENSOR";

// === 내부 측정 함수들 ===
float get_temperature(void) {
    return get_temperature_with_retry(3);  // 최대 3번 재시도
}

float get_humidity(void) {
    return get_humidity_with_retry(3);  // 최대 3번 재시도
}

// 재시도 로직이 포함된 온도 읽기 함수
float get_temperature_with_retry(int max_retries) {
    for (int retry = 0; retry < max_retries; retry++) {
        int16_t temperature = 0;
        int16_t humidity = 0;
        
        esp_err_t result = dht_read_data(DHT_SENSOR_TYPE, DHT_GPIO_PIN, &humidity, &temperature);
        if (result == ESP_OK) {
            return temperature / 10.0f;  // 0.1도 단위를 도 단위로 변환
        }
        
        // 재시도 전 잠시 대기 (센서 안정화)
        if (retry < max_retries - 1) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    return -999.0f;  // 모든 재시도 실패 시 반환값
}

// 재시도 로직이 포함된 습도 읽기 함수
float get_humidity_with_retry(int max_retries) {
    for (int retry = 0; retry < max_retries; retry++) {
        int16_t temperature = 0;
        int16_t humidity = 0;
        
        esp_err_t result = dht_read_data(DHT_SENSOR_TYPE, DHT_GPIO_PIN, &humidity, &temperature);
        if (result == ESP_OK) {
            return humidity / 10.0f;  // 0.1% 단위를 % 단위로 변환
        }
        
        // 재시도 전 잠시 대기 (센서 안정화)
        if (retry < max_retries - 1) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    return -999.0f;  // 모든 재시도 실패 시 반환값
}

bool read_temp_humid_data(float *temperature, float *humidity) {
    // 재시도 로직을 사용하여 더 안정적인 읽기
    *temperature = get_temperature_with_retry(3);
    *humidity = get_humidity_with_retry(3);
    
    // 둘 다 성공적으로 읽혔는지 확인
    if (*temperature > -999.0f && *humidity > -999.0f) {
        return true;
    }
    return false;
}

// === 센서 초기화 ===
void temp_humid_sensor_init(void) {
    ESP_LOGI(TAG, "온습도 센서 초기화 중...");
    ESP_LOGI(TAG, "DHT 센서 GPIO: %d", DHT_GPIO_PIN);
    ESP_LOGI(TAG, "센서 타입: %s", (DHT_SENSOR_TYPE == DHT_TYPE_DHT11) ? "DHT11" : "DHT22");
    
    // 로그 태스크 실행
    // xTaskCreate(temp_humid_log_task, "temp_humid_log_task", 2048, NULL, 5, NULL);
}

// === 2초마다 측정 및 로그 출력 ===
void temp_humid_log_task(void *pvParameters) {
    ESP_LOGI(TAG, "온습도 센서 측정 시작...");
    
    while (1) {
        float temperature, humidity;
        
        if (read_temp_humid_data(&temperature, &humidity)) {
            ESP_LOGI(TAG, "=== DHT 온습도 측정 ===");
            ESP_LOGI(TAG, "온도: %.1f°C", temperature);
            ESP_LOGI(TAG, "습도: %.1f%%", humidity);
            
            // 경고 메시지
            if (temperature > 30.0f) {
                ESP_LOGW(TAG, "고온 경고!");
            } else if (temperature < 10.0f) {
                ESP_LOGW(TAG, "저온 경고!");
            }
            
            if (humidity > 70.0f) {
                ESP_LOGW(TAG, "고습도 경고!");
            } else if (humidity < 30.0f) {
                ESP_LOGW(TAG, "저습도 경고!");
            }
        } else {
            ESP_LOGE(TAG, "센서 읽기 실패");
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000));  // 2초마다 반복
    }
}