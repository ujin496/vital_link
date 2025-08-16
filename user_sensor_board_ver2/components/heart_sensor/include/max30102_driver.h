#pragma once

#include <stdint.h>
#include "driver/i2c.h"
#include "esp_err.h"

// MAX30102 레지스터 주소 정의
#define MAX30102_I2C_ADDR         0x57

// 상태 레지스터
#define MAX30102_REG_INT_STATUS_1 0x00
#define MAX30102_REG_INT_STATUS_2 0x01
#define MAX30102_REG_INT_ENABLE_1 0x02
#define MAX30102_REG_INT_ENABLE_2 0x03

// FIFO 레지스터
#define MAX30102_REG_FIFO_WR_PTR  0x04
#define MAX30102_REG_FIFO_OVF_CNT 0x05
#define MAX30102_REG_FIFO_RD_PTR  0x06
#define MAX30102_REG_FIFO_DATA    0x07

// 설정 레지스터
#define MAX30102_REG_FIFO_CONFIG  0x08
#define MAX30102_REG_MODE_CONFIG  0x09
#define MAX30102_REG_SPO2_CONFIG  0x0A
#define MAX30102_REG_LED1_PA      0x0C  // IR LED
#define MAX30102_REG_LED2_PA      0x0D  // RED LED
#define MAX30102_REG_PILOT_PA     0x10
#define MAX30102_REG_MULTI_LED_CTRL1 0x11
#define MAX30102_REG_MULTI_LED_CTRL2 0x12

// 온도 센서
#define MAX30102_REG_TEMP_INT     0x1F
#define MAX30102_REG_TEMP_FRAC    0x20
#define MAX30102_REG_TEMP_CONFIG  0x21

// ID 레지스터
#define MAX30102_REG_REV_ID       0xFE
#define MAX30102_REG_PART_ID      0xFF

// 모드 설정값
#define MAX30102_MODE_HEART_RATE  0x02
#define MAX30102_MODE_SPO2        0x03
#define MAX30102_MODE_MULTI_LED   0x07

// 샘플링 설정값
#define MAX30102_SAMPLERATE_50    0x00
#define MAX30102_SAMPLERATE_100   0x01
#define MAX30102_SAMPLERATE_200   0x02
#define MAX30102_SAMPLERATE_400   0x03
#define MAX30102_SAMPLERATE_800   0x04
#define MAX30102_SAMPLERATE_1000  0x05
#define MAX30102_SAMPLERATE_1600  0x06
#define MAX30102_SAMPLERATE_3200  0x07

// ADC 해상도 설정값
#define MAX30102_ADCRANGE_2048    0x00
#define MAX30102_ADCRANGE_4096    0x01
#define MAX30102_ADCRANGE_8192    0x02
#define MAX30102_ADCRANGE_16384   0x03

// Pulse Width 설정값
#define MAX30102_PULSEWIDTH_69    0x00  // 13 bit
#define MAX30102_PULSEWIDTH_118   0x01  // 14 bit
#define MAX30102_PULSEWIDTH_215   0x02  // 15 bit
#define MAX30102_PULSEWIDTH_411   0x03  // 16 bit

/**
 * @brief MAX30102 설정 구조체
 */
typedef struct {
    uint8_t led_mode;           // LED 모드 (HR, SpO2, Multi-LED)
    uint8_t sample_rate;        // 샘플링 레이트 (50-3200 Hz)
    uint8_t pulse_width;        // 펄스 폭 (13-16 bit)
    uint8_t adc_range;          // ADC 범위
    uint8_t ir_current;         // IR LED 전류 (0-255, 0.2mA 단위)
    uint8_t red_current;        // RED LED 전류 (0-255, 0.2mA 단위)
    uint8_t sample_averaging;   // 평균화 샘플 수 (1, 2, 4, 8, 16, 32)
    bool fifo_rollover;         // FIFO 롤오버 활성화
} max30102_config_t;

/**
 * @brief MAX30102 FIFO 데이터 구조체
 */
typedef struct {
    uint32_t red;
    uint32_t ir;
    uint8_t samples_available;
    bool fifo_overflow;
} max30102_fifo_data_t;

/**
 * @brief MAX30102 센서 초기화
 * @param port I2C 포트 번호
 * @return ESP_OK 성공, ESP_FAIL 실패
 */
esp_err_t max30102_init(i2c_port_t port);

/**
 * @brief MAX30102 고급 초기화 (사용자 정의 설정)
 * @param port I2C 포트 번호
 * @param config 센서 설정
 * @return ESP_OK 성공, ESP_FAIL 실패
 */
esp_err_t max30102_init_advanced(i2c_port_t port, const max30102_config_t *config);

/**
 * @brief MAX30102 FIFO에서 데이터 읽기
 * @param red 적외선 LED 데이터 포인터
 * @param ir 적외선 LED 데이터 포인터
 * @return ESP_OK 성공, ESP_FAIL 실패
 */
esp_err_t max30102_read_fifo(uint32_t *red, uint32_t *ir);

/**
 * @brief MAX30102 FIFO에서 다중 샘플 읽기
 * @param fifo_data FIFO 데이터 구조체 포인터
 * @param max_samples 읽을 최대 샘플 수
 * @return 실제 읽은 샘플 수
 */
uint8_t max30102_read_fifo_multi(max30102_fifo_data_t *fifo_data, uint8_t max_samples);

/**
 * @brief 센서 상태 확인
 * @return ESP_OK 정상, ESP_FAIL 오류
 */
esp_err_t max30102_check_status(void);

/**
 * @brief LED 전류 설정
 * @param ir_current IR LED 전류 (0-255, 0.2mA 단위)
 * @param red_current RED LED 전류 (0-255, 0.2mA 단위)
 * @return ESP_OK 성공, ESP_FAIL 실패
 */
esp_err_t max30102_set_led_current(uint8_t ir_current, uint8_t red_current);

/**
 * @brief 온도 측정 시작
 * @return ESP_OK 성공, ESP_FAIL 실패
 */
esp_err_t max30102_start_temperature_measurement(void);

/**
 * @brief 온도 읽기
 * @param temperature 온도 값 포인터 (섭씨)
 * @return ESP_OK 성공, ESP_FAIL 실패
 */
esp_err_t max30102_read_temperature(float *temperature);

/**
 * @brief FIFO 클리어
 * @return ESP_OK 성공, ESP_FAIL 실패
 */
esp_err_t max30102_clear_fifo(void);

/**
 * @brief 사용 가능한 FIFO 샘플 수 확인
 * @return 사용 가능한 샘플 수
 */
uint8_t max30102_get_fifo_samples_available(void);
