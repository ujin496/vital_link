#include "sensor_data.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h> // Added for memset

static sensor_data_t current_data;
static SemaphoreHandle_t data_mutex;

void sensor_data_init(void) {
    data_mutex = xSemaphoreCreateMutex();
    // 유효성 플래그 초기화
    memset(&current_data.validity_flags, 0, sizeof(current_data.validity_flags));
}

void sensor_data_set_heart_rate(float hr) {
    if (xSemaphoreTake(data_mutex, portMAX_DELAY)) {
        current_data.heart_rate = hr;
        current_data.validity_flags.heart_rate_valid = 1;  // 유효성 플래그 설정
        xSemaphoreGive(data_mutex);
    }
}

void sensor_data_set_temperature(float temp) {
    if (xSemaphoreTake(data_mutex, portMAX_DELAY)) {
        current_data.temperature = temp;
        current_data.validity_flags.temperature_valid = 1;
        xSemaphoreGive(data_mutex);
    }
}

void sensor_data_set_spo2(int spo2) {
    if (xSemaphoreTake(data_mutex, portMAX_DELAY)) {
        current_data.spo2 = spo2;
        current_data.validity_flags.spo2_valid = 1;
        xSemaphoreGive(data_mutex);
    }
}

void sensor_data_set_steps(int steps) {
    if (xSemaphoreTake(data_mutex, portMAX_DELAY)) {
        current_data.steps = steps;
        current_data.validity_flags.steps_valid = 1;
        xSemaphoreGive(data_mutex);
    }
}

void sensor_data_set_fall_detected(int fall) {
    if (xSemaphoreTake(data_mutex, portMAX_DELAY)) {
        current_data.fall_detected = fall;
        current_data.validity_flags.fall_detected_valid = 1;
        xSemaphoreGive(data_mutex);
    }
}

void sensor_data_set_timestamp(int64_t timestamp_ms) {
    if (xSemaphoreTake(data_mutex, portMAX_DELAY)) {
        current_data.timestamp_ms = timestamp_ms;
        xSemaphoreGive(data_mutex);
    }
}

void sensor_data_set_location(uint16_t major, uint16_t minor, int rssi) {
    if (xSemaphoreTake(data_mutex, portMAX_DELAY)) {
        current_data.location.major = major;
        current_data.location.minor = minor;
        current_data.location.rssi = rssi;
        current_data.validity_flags.location_valid = 1;
        xSemaphoreGive(data_mutex);
    }
}

sensor_data_t sensor_data_get_snapshot(void) {
    sensor_data_t copy;
    if (xSemaphoreTake(data_mutex, portMAX_DELAY)) {
        copy = current_data;
        xSemaphoreGive(data_mutex);
    }
    return copy;
}

// 유효한 측정값이 있는지 확인
int sensor_data_has_valid_measurements(void) {
    sensor_data_t snapshot = sensor_data_get_snapshot();
    return (snapshot.validity_flags.heart_rate_valid ||
            snapshot.validity_flags.temperature_valid ||
            snapshot.validity_flags.spo2_valid ||
            snapshot.validity_flags.steps_valid ||
            snapshot.validity_flags.fall_detected_valid ||
            snapshot.validity_flags.location_valid);
}

// 유효한 센서 개수 반환
int sensor_data_get_valid_count(void) {
    sensor_data_t snapshot = sensor_data_get_snapshot();
    int count = 0;
    count += snapshot.validity_flags.heart_rate_valid;
    count += snapshot.validity_flags.temperature_valid;
    count += snapshot.validity_flags.spo2_valid;
    count += snapshot.validity_flags.steps_valid;
    count += snapshot.validity_flags.fall_detected_valid;
    count += snapshot.validity_flags.location_valid;
    return count;
}
