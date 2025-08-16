#include "sensor_data.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static sensor_data_t current_data;
static SemaphoreHandle_t data_mutex;

// 위치 정보를 위한 전역 변수
static int location_major = 0;
static int location_minor = 0;
static int location_rssi = 0;

void sensor_data_init(void) {
    data_mutex = xSemaphoreCreateMutex();
    // 초기값 설정
    current_data.temperature = 0.0f;
    current_data.humidity = 0.0f;
    current_data.tvoc = 0.0f;
    current_data.rs = 0.0f;
    current_data.ratio = 0.0f;
    current_data.lux = 0.0f;
    current_data.timestamp_ms = 0;
}

void sensor_data_set_temperature(float temp) {
    if (xSemaphoreTake(data_mutex, portMAX_DELAY)) {
        current_data.temperature = temp;
        xSemaphoreGive(data_mutex);
    }
}

void sensor_data_set_humidity(float humidity) {
    if (xSemaphoreTake(data_mutex, portMAX_DELAY)) {
        current_data.humidity = humidity;
        xSemaphoreGive(data_mutex);
    }
}

void sensor_data_set_tvoc(float tvoc) {
    if (xSemaphoreTake(data_mutex, portMAX_DELAY)) {
        current_data.tvoc = tvoc;
        xSemaphoreGive(data_mutex);
    }
}

void sensor_data_set_rs(float rs) {
    if (xSemaphoreTake(data_mutex, portMAX_DELAY)) {
        current_data.rs = rs;
        xSemaphoreGive(data_mutex);
    }
}

void sensor_data_set_ratio(float ratio) {
    if (xSemaphoreTake(data_mutex, portMAX_DELAY)) {
        current_data.ratio = ratio;
        xSemaphoreGive(data_mutex);
    }
}

void sensor_data_set_lux(float lux) {
    if (xSemaphoreTake(data_mutex, portMAX_DELAY)) {
        current_data.lux = lux;
        xSemaphoreGive(data_mutex);
    }
}

void sensor_data_set_timestamp(int64_t timestamp_ms) {
    if (xSemaphoreTake(data_mutex, portMAX_DELAY)) {
        current_data.timestamp_ms = timestamp_ms;
        xSemaphoreGive(data_mutex);
    }
}

// 공통 구조체 데이터 보호 위해 snapshot 찍어서 전송
sensor_data_t sensor_data_get_snapshot(void) {
    sensor_data_t copy;
    if (xSemaphoreTake(data_mutex, portMAX_DELAY)) {
        copy = current_data;
        xSemaphoreGive(data_mutex);
    }
    return copy;
}

// 기존 센서 데이터를 새로운 InfluxDB 형식으로 변환
void sensor_data_convert_to_influx(const sensor_data_t* source, influx_sensor_data_t* dest, const char* device_id) {
    if (source == NULL || dest == NULL || device_id == NULL) return;
    
    // measurement와 device_id 설정
    strncpy(dest->measurement, "sensor_data", sizeof(dest->measurement) - 1);
    dest->measurement[sizeof(dest->measurement) - 1] = '\0';
    
    strncpy(dest->device_id, device_id, sizeof(dest->device_id) - 1);
    dest->device_id[sizeof(dest->device_id) - 1] = '\0';
    
    // 센서 데이터 복사
    dest->temperature = source->temperature;
    dest->humidity = source->humidity;
    dest->tvoc = source->tvoc;
    dest->rs = source->rs;
    dest->ratio = source->ratio;
    dest->lux = source->lux;
    dest->timestamp_ms = source->timestamp_ms;
    
    // 위치 정보 설정
    dest->major = location_major;
    dest->minor = location_minor;
    dest->rssi = location_rssi;
}

// 위치 정보 설정 함수
void sensor_data_set_location_data(int major, int minor, int rssi) {
    if (xSemaphoreTake(data_mutex, portMAX_DELAY)) {
        location_major = major;
        location_minor = minor;
        location_rssi = rssi;
        xSemaphoreGive(data_mutex);
    }
}
