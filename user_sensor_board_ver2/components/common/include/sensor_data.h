#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <stdint.h>

// 위치 정보 구조체 추가
typedef struct {
    uint16_t major;
    uint16_t minor;
    int rssi;
} location_data_t;

typedef struct {
    float heart_rate;     // 단위: bpm (beats per minute)
    float temperature;    // 단위: °C
    int spo2;             // 단위: %, 산소포화도 (정수로 표현)
    int steps;            // 걸음 수 (누적 정수값)
    int fall_detected;    // 낙상 감지 (Boolean)
    int64_t timestamp_ms; // UNIX timestamp in milliseconds
    location_data_t location; // 위치 정보 추가
    
    // 유효성 플래그 추가
    struct {
        uint8_t heart_rate_valid : 1;
        uint8_t temperature_valid : 1;
        uint8_t spo2_valid : 1;
        uint8_t steps_valid : 1;
        uint8_t fall_detected_valid : 1;
        uint8_t location_valid : 1;
        uint8_t reserved : 2;
    } validity_flags;
} sensor_data_t;

// 초기화 함수 (예: mutex 생성 등)
void sensor_data_init(void);

// 각 항목별 setter 함수
void sensor_data_set_heart_rate(float hr);
void sensor_data_set_temperature(float temp);
void sensor_data_set_spo2(int spo2);
void sensor_data_set_steps(int steps);
void sensor_data_set_fall_detected(int fall);
void sensor_data_set_timestamp(int64_t timestamp);
void sensor_data_set_location(uint16_t major, uint16_t minor, int rssi);

// 전체 snapshot 가져오기
sensor_data_t sensor_data_get_snapshot(void);

// 유효성 검사 함수 추가
int sensor_data_has_valid_measurements(void);
int sensor_data_get_valid_count(void);

#endif  // SENSOR_DATA_H
