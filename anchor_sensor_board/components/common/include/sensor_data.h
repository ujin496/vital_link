#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <stdint.h>

typedef struct {
    float temperature;    // 온도 (temp_humid_sensor에서)
    float humidity;       // 습도 (temp_humid_sensor에서)
    float tvoc;          // TVOC 값 (tvoc_sensor에서)
    float rs;            // 저항값 (tvoc_sensor에서)
    float ratio;         // 비율값 (tvoc_sensor에서)
    float lux;           // 조도 값 (light_sensor에서)
    int64_t timestamp_ms;  // esp_timer_get_time() 사용
} sensor_data_t;

// 새로운 InfluxDB 형식의 센서 데이터 구조체
typedef struct {
    char measurement[32];     // "sensor_data"
    char device_id[16];       // "dev01" 등
    float temperature;        // 온도값
    float humidity;          // 습도값
    float tvoc;             // TVOC값
    float rs;               // 저항값
    float ratio;            // 비율값
    float lux;              // 조도값
    int major;              // 비콘 major (기본값 0)
    int minor;              // 비콘 minor (기본값 0) 
    int rssi;               // RSSI (기본값 0)
    int64_t timestamp_ms;    // 타임스탬프
} influx_sensor_data_t;

// 초기화 함수 (예: mutex 생성 등)
void sensor_data_init(void);

// 각 항목별 setter 함수
void sensor_data_set_temperature(float temp);
void sensor_data_set_humidity(float humidity);
void sensor_data_set_tvoc(float tvoc);
void sensor_data_set_rs(float rs);
void sensor_data_set_ratio(float ratio);
void sensor_data_set_lux(float lux);
void sensor_data_set_timestamp(int64_t timestamp);

// 전체 snapshot 가져오기
sensor_data_t sensor_data_get_snapshot(void);

// 새로운 InfluxDB 형식 관련 함수들
void sensor_data_convert_to_influx(const sensor_data_t* source, influx_sensor_data_t* dest, const char* device_id);
void sensor_data_set_location_data(int major, int minor, int rssi);

#endif  // SENSOR_DATA_H
