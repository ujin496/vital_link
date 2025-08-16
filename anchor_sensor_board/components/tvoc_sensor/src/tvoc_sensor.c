#include "tvoc_sensor.h"

#include <stdio.h>
#include <math.h>
#include "driver/gpio.h"
#include "driver/adc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define MQ135_ADC_CHANNEL ADC1_CHANNEL_6   // GPIO34 (AO)
#define MQ135_DIGITAL_PIN GPIO_NUM_2       // DO 핀
#define MQ135_RLOAD       10000.0          // 10kΩ
#define MQ135_RZERO       10.0             // 보정용 R0 값 (직접 보정 필요)
#define TVOC_CALIBRATION_FACTOR 1.5f

/*
mq135

ao 34

do 2
*/

static const char *TAG = "TVOC_SENSOR";

// === 내부 측정 함수들 ===
float mq135_get_rs(void) {
    int adc_raw = adc1_get_raw(MQ135_ADC_CHANNEL);
    if (adc_raw == 0) return -1;
    float voltage = ((float)adc_raw / 4095.0) * 3.3;
    float rs = ((3.3 - voltage) * MQ135_RLOAD) / voltage;
    return rs / 1000.0;  // kΩ 단위
}

float mq135_get_ratio(float rs) {
    return rs / MQ135_RZERO;
}

float mq135_get_tvoc_ppb(float ratio) {
    if (ratio <= 0) return -1;
    return 116.6020682f * powf(ratio, -2.769034857f);
}

bool mq135_detect_gas(void) {
    return gpio_get_level(MQ135_DIGITAL_PIN) == 0; // LOW면 감지됨
}

// === 센서 초기화 ===
void tvoc_sensor_init(void) {
    // ADC 설정
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(MQ135_ADC_CHANNEL, ADC_ATTEN_DB_12);

    // 디지털 핀 설정
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << MQ135_DIGITAL_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // 로그 태스크 실행
    // xTaskCreate(tvoc_log_task, "tvoc_log_task", 2048, NULL, 5, NULL);
}

// === 5초마다 측정 및 로그 출력 ===
void tvoc_log_task(void *pvParameters) {
    while (1) {
        float rs = mq135_get_rs();
        if (rs < 0) {
            ESP_LOGW(TAG, "센서 오류: ADC 값이 0입니다.");
        } else {
            float ratio = mq135_get_ratio(rs);
            float tvoc_raw = mq135_get_tvoc_ppb(ratio)/TVOC_CALIBRATION_FACTOR;
            float tvoc = mq135_get_tvoc_ppb(ratio);
            bool detected = mq135_detect_gas();

            ESP_LOGI(TAG, "=== MQ135 TVOC 측정 ===");
            // ESP_LOGI(TAG, "ADC 기반 Rs: %.2f kΩ", rs);
            // ESP_LOGI(TAG, "Rs/R0 비율: %.3f", ratio);
            ESP_LOGI(TAG, "TVOC: %.1f ppb", tvoc);
            ESP_LOGI(TAG, "RAW TVOC: %.1f ppb", tvoc_raw);
            if (tvoc >=50){
                ESP_LOGI(TAG, "실내 공기질 위험!");
            }
            // ESP_LOGI(TAG, "디지털 감지: %s", detected ? "감지됨" : "정상");
        }

        vTaskDelay(pdMS_TO_TICKS(5000));  // 1초마다 반복
    }
}
