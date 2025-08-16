#pragma once

#include <stdint.h>
#include <stdbool.h>

// SpO2 의료적 기준 임계값
#define SPO2_NORMAL_MIN 95          // 정상 범위 최소값
#define SPO2_HYPOXIA_WARNING 90     // 저산소증 주의 임계값
#define SPO2_HYPOXIA_DANGER 80      // 저산소증 위험 임계값
#define SPO2_SEVERE_HYPOXIA 75      // 매우 심한 저산소증 임계값

/**
 * @brief SpO2 상태 열거형
 */
typedef enum {
    SPO2_STATUS_NORMAL = 0,         // 95-100%: 정상
    SPO2_STATUS_WARNING,            // 90-94%: 저산소증 주의
    SPO2_STATUS_DANGER,             // 80-89%: 저산소증 위험
    SPO2_STATUS_SEVERE,             // 75-79%: 매우 심한 저산소증
    SPO2_STATUS_INVALID             // 측정 불가 또는 비정상
} spo2_status_t;

/**
 * @brief 심박수 및 SpO2 계산을 위한 데이터 구조체
 */
typedef struct {
    float heart_rate;      // 심박수 (bpm)
    int spo2;              // 산소포화도 (%)
    bool valid_data;       // 유효한 데이터 여부
    float signal_quality;  // 신호 품질 지표 (0.0-1.0)
    float perfusion_index; // PI 값 (혈액 순환 지표)
    float r_ratio;         // R 비율 (SpO2 계산용)
    spo2_status_t spo2_status; // SpO2 의료적 상태
} heart_rate_data_t;

/**
 * @brief 신호 품질 평가 결과
 */
typedef struct {
    float perfusion_index;  // PI 값
    float snr_estimate;     // 신호 대 잡음비 추정값
    bool contact_detected;  // 센서 접촉 감지
    bool quality_good;      // 전체 품질 평가
    float red_dc;           // RED LED DC 성분
    float ir_dc;            // IR LED DC 성분
    float red_ac_rms;       // RED LED AC RMS 값
    float ir_ac_rms;        // IR LED AC RMS 값
} signal_quality_t;

/**
 * @brief 필터링된 신호 데이터
 */
typedef struct {
    float red_filtered;     // 필터링된 RED 신호
    float ir_filtered;      // 필터링된 IR 신호
    float red_dc;           // RED DC 성분
    float ir_dc;            // IR DC 성분
    float red_ac;           // RED AC 성분
    float ir_ac;            // IR AC 성분
} filtered_signal_t;

/**
 * @brief MAX30102 센서 데이터를 처리하여 심박수와 SpO2를 계산
 * @param red RED LED 센서 값
 * @param ir IR LED 센서 값
 * @return 계산된 심박수 및 SpO2 데이터
 */
heart_rate_data_t calculate_heart_rate_and_spo2(uint32_t red, uint32_t ir);

/**
 * @brief 심박수 및 SpO2 계산기 초기화
 */
void heart_rate_calculator_init(void);

/**
 * @brief 심박수 및 SpO2 계산기 리셋
 */
void heart_rate_calculator_reset(void);

/**
 * @brief 최근 심박수 반환
 * @return 최근 심박수 (bpm), 유효하지 않으면 0.0
 */
float hr_get_latest(void);

/**
 * @brief 최근 SpO2 반환
 * @return 최근 SpO2 (%), 유효하지 않으면 0
 */
int hr_get_latest_spo2(void);

/**
 * @brief 최근 SpO2 상태 반환
 * @return SpO2 의료적 상태
 */
spo2_status_t hr_get_spo2_status(void);

/**
 * @brief SpO2 상태를 문자열로 변환
 * @param status SpO2 상태
 * @return 상태 문자열
 */
const char* hr_get_spo2_status_string(spo2_status_t status);

/**
 * @brief 심박수 샘플 업데이트 (빠른 샘플링용)
 * @param red RED LED 센서 값
 * @param ir IR LED 센서 값
 */
void hr_update_sample(uint32_t red, uint32_t ir);

/**
 * @brief 현재 신호 품질 평가 반환
 * @return 신호 품질 평가 결과
 */
signal_quality_t hr_get_signal_quality(void);

/**
 * @brief 최근 심박수가 유효한지 확인
 * @return true if valid, false otherwise
 */
bool hr_is_latest_valid(void);

/**
 * @brief 현재 필터링된 신호 데이터 반환
 * @return 필터링된 신호 데이터
 */
filtered_signal_t hr_get_filtered_signals(void);

/**
 * @brief 신호 품질 기반 데이터 검증
 * @param red RED LED 값
 * @param ir IR LED 값
 * @return true if data is valid, false otherwise
 */
bool hr_validate_signal_quality(uint32_t red, uint32_t ir);

/**
 * @brief LED 전류 자동 조정 (AGC - Automatic Gain Control)
 * @param current_red_dc 현재 RED DC 값
 * @param current_ir_dc 현재 IR DC 값
 * @return true if adjustment needed, false otherwise
 */
bool hr_auto_adjust_led_current(uint32_t current_red_dc, uint32_t current_ir_dc);