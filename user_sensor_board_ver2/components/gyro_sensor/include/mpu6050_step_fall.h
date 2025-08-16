#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// 낙상 방향 enum
typedef enum {
    FALL_DIR_NONE = 0,
    FALL_DIR_FRONT,          // 앞
    FALL_DIR_BACK,           // 뒤
    FALL_DIR_LEFT,           // 좌
    FALL_DIR_RIGHT,          // 우
    FALL_DIR_FRONT_LEFT,     // 앞-좌
    FALL_DIR_FRONT_RIGHT,    // 앞-우
    FALL_DIR_BACK_LEFT,      // 뒤-좌
    FALL_DIR_BACK_RIGHT      // 뒤-우
} fall_direction_t;

// 낙상 결과 구조체
typedef struct {
    bool fall_detected;           // 낙상 감지 여부
    fall_direction_t direction;   // 낙상 방향
    float fall_angle_deg;         // 낙상 각도 (도)
    float ax_g;                   // X축 가속도 (g)
    float ay_g;                   // Y축 가속도 (g)
    float roll_deg;               // Roll 각도 (도)
    float pitch_deg;              // Pitch 각도 (도)
} fall_result_t;

// 걸음 수 및 낙상 감지를 위한 컨텍스트 구조체
typedef struct {
    // 필터 및 임계값 파라미터 (걸음 수 감지용)
    float lpf_a;                    // 중력 추정을 위한 LPF 알파 (예: 0.02 @100Hz)
    float ema_a;                    // 동적 임계값 베이스라인을 위한 EMA 알파 (예: 0.01)
    float dyn_k;                    // 동적 임계값 게인 (예: 1.5)
    float step_min_interval_ms;     // 재진입 지연 시간 (예: 300ms)
    float gyro_gate_dps;           // 자이로 RMS가 이 값보다 크면 스텝 무시 (예: 80 dps)

    // 논문 기반 낙상 감지 파라미터
    float accel_threshold_g;        // 가속도 임계값 (논문: 2g)
    float angle_threshold_deg;      // 각도 임계값 (논문: 30° 또는 45°)

    // 런타임 상태
    float g_est_x, g_est_y, g_est_z;  // 중력 추정값
    float ema_abs_a;                   // 동적 임계값을 위한 베이스라인
    uint32_t last_step_ms;             // 마지막 스텝 시간

    // 낙상 감지 상태 (간소화)
    bool fall_detected;                // 낙상 감지 플래그
    uint32_t fall_reset_time_ms;       // 낙상 리셋 시간
} step_fall_ctx_t;

// 함수 선언
void step_fall_init(step_fall_ctx_t* ctx, float sample_hz);

bool step_fall_detect_step(step_fall_ctx_t* ctx,
                           int16_t ax_raw, int16_t ay_raw, int16_t az_raw,
                           int16_t gx_raw, int16_t gy_raw, int16_t gz_raw,
                           uint32_t now_ms);

fall_result_t step_fall_detect_fall(step_fall_ctx_t* ctx,
                                   int16_t ax_raw, int16_t ay_raw, int16_t az_raw,
                                   int16_t gx_raw, int16_t gy_raw, int16_t gz_raw,
                                   uint32_t now_ms);

void step_fall_reset_fall(step_fall_ctx_t* ctx);

// Roll, Pitch 각도 계산 헬퍼 함수
float calculate_roll_angle(float ax_g, float ay_g, float az_g);
float calculate_pitch_angle(float ax_g, float ay_g, float az_g);

// 낙상 방향 판단 헬퍼 함수
fall_direction_t determine_fall_direction(float roll_deg, float pitch_deg);
