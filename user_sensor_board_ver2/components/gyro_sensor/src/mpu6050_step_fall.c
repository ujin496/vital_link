#include "mpu6050_step_fall.h"
#include <math.h>
#include "esp_log.h"

static const char *TAG = "STEP_FALL";

// MPU6050 스케일 (±2g, ±2000 dps 가정) - 필요 시 실제 설정값에 맞춰 수정
#define ACC_LSB_PER_G     16384.0f
#define GYRO_LSB_PER_DPS  16.4f

// 수학 상수
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// 유틸리티 함수들
static inline float sqr(float x) { return x * x; }
static inline float clampf(float v, float lo, float hi) { 
    return v < lo ? lo : (v > hi ? hi : v); 
}

// 낙상 이벤트 쿨다운 관리
static uint32_t last_fall_time = 0;

/**
 * @brief Roll 각도 계산 (φ_R)
 * @param ax_g, ay_g, az_g 가속도 값 (g 단위)
 * @return Roll 각도 (도)
 */
float calculate_roll_angle(float ax_g, float ay_g, float az_g) {
    // Roll = atan2(ay, sqrt(ax^2 + az^2))
    float denominator = sqrtf(ax_g * ax_g + az_g * az_g);
    if (denominator < 1e-6f) return 0.0f;
    
    float roll_rad = atan2f(ay_g, denominator);
    return roll_rad * 180.0f / M_PI;
}

/**
 * @brief Pitch 각도 계산 (φ_P)
 * @param ax_g, ay_g, az_g 가속도 값 (g 단위)
 * @return Pitch 각도 (도)
 */
float calculate_pitch_angle(float ax_g, float ay_g, float az_g) {
    // Pitch = atan2(-ax, sqrt(ay^2 + az^2))
    float denominator = sqrtf(ay_g * ay_g + az_g * az_g);
    if (denominator < 1e-6f) return 0.0f;
    
    float pitch_rad = atan2f(-ax_g, denominator);
    return pitch_rad * 180.0f / M_PI;
}

/**
 * @brief 낙상 방향 판단 (논문 기반)
 * @param roll_deg Roll 각도 (도)
 * @param pitch_deg Pitch 각도 (도)
 * @return 낙상 방향
 */
fall_direction_t determine_fall_direction(float roll_deg, float pitch_deg) {
    // 논문의 공식: θ_fall = tan^(-1)(φ_R / φ_P)
    if (fabsf(pitch_deg) < 1e-6f && fabsf(roll_deg) < 1e-6f) {
        return FALL_DIR_NONE;
    }
    
    // 8방향 분류를 위한 각도 계산
    float fall_angle_rad = atan2f(roll_deg, pitch_deg);
    float fall_angle_deg = fall_angle_rad * 180.0f / M_PI;
    
    // 각도를 0~360도 범위로 정규화
    while (fall_angle_deg < 0) fall_angle_deg += 360.0f;
    while (fall_angle_deg >= 360) fall_angle_deg -= 360.0f;
    
    // 8방향 분류 (45도씩 나누어)
    if (fall_angle_deg >= 337.5f || fall_angle_deg < 22.5f) {
        return FALL_DIR_FRONT;      // 앞 (0°)
    } else if (fall_angle_deg >= 22.5f && fall_angle_deg < 67.5f) {
        return FALL_DIR_FRONT_RIGHT; // 앞-우 (45°)
    } else if (fall_angle_deg >= 67.5f && fall_angle_deg < 112.5f) {
        return FALL_DIR_RIGHT;       // 우 (90°)
    } else if (fall_angle_deg >= 112.5f && fall_angle_deg < 157.5f) {
        return FALL_DIR_BACK_RIGHT;  // 뒤-우 (135°)
    } else if (fall_angle_deg >= 157.5f && fall_angle_deg < 202.5f) {
        return FALL_DIR_BACK;        // 뒤 (180°)
    } else if (fall_angle_deg >= 202.5f && fall_angle_deg < 247.5f) {
        return FALL_DIR_BACK_LEFT;   // 뒤-좌 (225°)
    } else if (fall_angle_deg >= 247.5f && fall_angle_deg < 292.5f) {
        return FALL_DIR_LEFT;        // 좌 (270°)
    } else { // 292.5f ~ 337.5f
        return FALL_DIR_FRONT_LEFT;  // 앞-좌 (315°)
    }
}

/**
 * @brief 걸음 수 및 낙상 감지 초기화
 * @param ctx 컨텍스트 구조체
 * @param sample_hz 샘플링 주파수 (Hz)
 */
void step_fall_init(step_fall_ctx_t* ctx, float sample_hz) {
    if (ctx == NULL) return;
    
    // 구조체 초기화
    *ctx = (step_fall_ctx_t){0};
    
    // 기본 파라미터 (100Hz 기준) - 스텝 감지용
    ctx->lpf_a = 0.02f;
    ctx->ema_a = 0.01f;
    ctx->dyn_k = 1.0f;
    ctx->step_min_interval_ms = 220.0f;
    ctx->gyro_gate_dps = 120.0f;

    // 논문 기반 낙상 감지 파라미터 (균형잡힌 설정)
    ctx->accel_threshold_g = 2.8f;      // 3.5g → 2.8g (적당히 완화)
    ctx->angle_threshold_deg = 50.0f;   // 50° → 40° (적당히 완화)

    // 초기 상태 설정
    ctx->fall_detected = false;
    ctx->fall_reset_time_ms = 0;
    
    ESP_LOGI(TAG, "엄격한 이벤트 기반 낙상 감지 알고리즘 초기화 완료 (샘플링: %.1f Hz)", sample_hz);
    ESP_LOGI(TAG, "낙상 조건: 1)매우큰충격≥5.0g OR 2)강한충격≥4.0g+기울기≥30° OR 3)충격≥3.0g+기울기≥45°");
    ESP_LOGI(TAG, "쿨다운: 10초간 재감지 방지");
    ESP_LOGI(TAG, "걸음 수 감지: dyn_k=%.1f, 간격=%dms, 자이로게이트=%.0fdps", 
             ctx->dyn_k, (int)ctx->step_min_interval_ms, ctx->gyro_gate_dps);
}

/**
 * @brief 중력 추정 (1차 LPF 사용)
 * @param ctx 컨텍스트
 * @param ax_g, ay_g, az_g 가속도 값 (g 단위)
 */
static void estimate_gravity(step_fall_ctx_t* ctx, float ax_g, float ay_g, float az_g) {
    // 1차 LPF로 중력 추정 (정지/저주파 추정)
    ctx->g_est_x = (1 - ctx->lpf_a) * ctx->g_est_x + ctx->lpf_a * ax_g;
    ctx->g_est_y = (1 - ctx->lpf_a) * ctx->g_est_y + ctx->lpf_a * ay_g;
    ctx->g_est_z = (1 - ctx->lpf_a) * ctx->g_est_z + ctx->lpf_a * az_g;
}

/**
 * @brief 걸음 수 감지 (기존 로직 유지)
 * @param ctx 컨텍스트
 * @param ax_raw, ay_raw, az_raw 가속도 원시값 (LSB)
 * @param gx_raw, gy_raw, gz_raw 자이로 원시값 (LSB)
 * @param now_ms 현재 시간 (ms)
 * @return true: 스텝 감지됨, false: 스텝 아님
 */
bool step_fall_detect_step(step_fall_ctx_t* ctx,
                           int16_t ax_raw, int16_t ay_raw, int16_t az_raw,
                           int16_t gx_raw, int16_t gy_raw, int16_t gz_raw,
                           uint32_t now_ms) {
    if (ctx == NULL) return false;

    // 스케일 변환
    float ax_g = ax_raw / ACC_LSB_PER_G;
    float ay_g = ay_raw / ACC_LSB_PER_G;
    float az_g = az_raw / ACC_LSB_PER_G;
    float gx_dps = gx_raw / GYRO_LSB_PER_DPS;
    float gy_dps = gy_raw / GYRO_LSB_PER_DPS;
    float gz_dps = gz_raw / GYRO_LSB_PER_DPS;

    // 중력 추정 갱신
    estimate_gravity(ctx, ax_g, ay_g, az_g);

    // ===== 팔목 착용 XY축 전용 스텝 검출 (기존 로직 유지) =====
    
    // 1. XY축만 사용 (Z축 완전 제외)
    float lx = ax_g - ctx->g_est_x;  // X축 움직임
    float ly = ay_g - ctx->g_est_y;  // Y축 움직임  
    
    // XY축만의 선형 가속도 (걷기의 주요 신호)
    float xy_motion = sqrtf(lx * lx + ly * ly);
    
    // 2. XY축만의 가속도 변화량
    static float prev_ax = 0.0f, prev_ay = 0.0f, prev_az = 0.0f;
    float delta_ax = fabsf(ax_g - prev_ax);
    float delta_ay = fabsf(ay_g - prev_ay);
    
    // XY축 변화량만 사용
    float xy_delta = sqrtf(delta_ax * delta_ax + delta_ay * delta_ay);
    
    // 3. XY축 자이로만 사용 (Z축 자이로 제외)
    float xy_gyro = sqrtf(gx_dps * gx_dps + gy_dps * gy_dps);
    
    // 4. 걷기 신호 계산 (XY축만 사용, Z축 완전 제외)
    float walk_signal = xy_motion * 1.8f + xy_delta * 1.5f;
    
    // 5. 동적 임계값 계산 (XY축 전용)
    ctx->ema_abs_a = (1 - ctx->ema_a) * ctx->ema_abs_a + ctx->ema_a * walk_signal;
    float step_thresh = ctx->dyn_k * (ctx->ema_abs_a + 0.12f);
    
    // 6. 스텝 검출 (XY축 전용)
    static bool above = false;
    static float peak_value = 0.0f;

    // 히스테리시스
    float th_hi = step_thresh;
    float th_lo = step_thresh * 0.6f;

    // 스텝 후보 시작 조건 (XY축만 사용)
    bool sufficient_xy_motion = (walk_signal > th_hi);
    bool xy_gyro_ok = (xy_gyro < ctx->gyro_gate_dps);
    bool min_xy_activity = (xy_motion > 0.08f);
    
    if (!above && sufficient_xy_motion && xy_gyro_ok && min_xy_activity) {
        above = true;
        peak_value = walk_signal;
        ESP_LOGD(TAG, "스텝 후보 시작 (XY신호: %.3f, XY움직임: %.3f, XY변화: %.3f, XY자이로: %.1f)", 
                 walk_signal, xy_motion, xy_delta, xy_gyro);
    } else if (above) {
        // 피크 값 업데이트
        if (walk_signal > peak_value) {
            peak_value = walk_signal;
        }
        
        // 하강 에지로 피크 확정
        if (walk_signal < th_lo) {
            above = false;
            
            // 최소 간격 확인
            if (now_ms - ctx->last_step_ms >= (uint32_t)ctx->step_min_interval_ms) {
                // 피크 크기 확인
                float peak_magnitude = peak_value - th_lo;
                
                // XY축 전용 검증
                bool valid_xy_step = (peak_magnitude > 0.06f) &&
                                    (xy_motion > 0.05f) &&
                                    (xy_delta > 0.05f);
                
                if (valid_xy_step) {
                    ctx->last_step_ms = now_ms;
                    ESP_LOGI(TAG, "스텝 감지! (XY신호: %.3f, XY움직임: %.3f, XY변화: %.3f, 피크: %.3f)", 
                             walk_signal, xy_motion, xy_delta, peak_magnitude);
                    return true;
                } else {
                    ESP_LOGD(TAG, "스텝 후보 무효 (피크: %.3f, XY움직임: %.3f, XY변화: %.3f)", 
                             peak_magnitude, xy_motion, xy_delta);
                }
            }
            
            peak_value = 0.0f;
        }
    }
    
    // 이전 값 저장
    prev_ax = ax_g;
    prev_ay = ay_g;
    prev_az = az_g;
    
    return false;
}

/**
 * @brief 논문 기반 낙상 감지 및 방향 판단
 * @param ctx 컨텍스트
 * @param ax_raw, ay_raw, az_raw 가속도 원시값 (LSB)
 * @param gx_raw, gy_raw, gz_raw 자이로 원시값 (LSB)
 * @param now_ms 현재 시간 (ms)
 * @return 낙상 결과 구조체
 */
fall_result_t step_fall_detect_fall(step_fall_ctx_t* ctx,
                                   int16_t ax_raw, int16_t ay_raw, int16_t az_raw,
                                   int16_t gx_raw, int16_t gy_raw, int16_t gz_raw,
                                   uint32_t now_ms) {
    fall_result_t result = {0};
    
    if (ctx == NULL) return result;

    // 스케일 변환
    float ax_g = ax_raw / ACC_LSB_PER_G;
    float ay_g = ay_raw / ACC_LSB_PER_G;
    float az_g = az_raw / ACC_LSB_PER_G;

    // Roll, Pitch 각도 계산
    float roll_deg = calculate_roll_angle(ax_g, ay_g, az_g);
    float pitch_deg = calculate_pitch_angle(ax_g, ay_g, az_g);
    
    // 결과 구조체에 기본 정보 저장
    result.ax_g = ax_g;
    result.ay_g = ay_g;
    result.roll_deg = roll_deg;
    result.pitch_deg = pitch_deg;
    result.fall_angle_deg = atan2f(roll_deg, pitch_deg) * 180.0f / M_PI;
    
    // ===== 이벤트 기반 낙상 감지 (넘어지는 순간만 감지) =====
    
    // 최근 낙상 감지를 방지하기 위한 쿨다운 (10초)
    if (now_ms - last_fall_time < 10000) {
        return result;  // 쿨다운 중이면 감지 안함
    }
    
    // 가속도 및 각도 조건 확인
    bool accel_x_exceed = (fabsf(ax_g) >= ctx->accel_threshold_g);
    bool accel_y_exceed = (fabsf(ay_g) >= ctx->accel_threshold_g);
    bool pitch_exceed = (fabsf(pitch_deg) >= ctx->angle_threshold_deg);
    bool roll_exceed = (fabsf(roll_deg) >= ctx->angle_threshold_deg);
    
    // 총 가속도 및 각도 변화 계산
    float total_accel = sqrtf(ax_g * ax_g + ay_g * ay_g + az_g * az_g);
    float total_angle_change = sqrtf(pitch_deg * pitch_deg + roll_deg * roll_deg);
    
    // 낙상 이벤트 감지 조건들 (매우 엄격하게 수정)
    
    // 1. 매우 큰 충격 (확실한 낙상)
    bool extreme_impact = (total_accel >= 5.0f);  // 5.0g 이상의 매우 큰 충격
    
    // // 2. 충격 + 기울기 동시 발생 (실제 넘어짐)
    // bool significant_impact = (total_accel >= 3.0f);  // 중간 정도 충격
    // bool significant_tilt = (total_angle_change >= 45.0f);  // 중간 정도 기울기
    // bool impact_with_tilt = significant_impact && significant_tilt;
    
    // 3. 강한 충격 + 매우 큰 기울기 동시 발생 (확실한 낙상)
    bool strong_impact = (total_accel >= 3.3f);  // 강한 충격
    bool very_large_tilt = (total_angle_change >= 45.0f);  // 매우 큰 기울기 (실제 낙상 수준)
    bool strong_impact_with_tilt = strong_impact && very_large_tilt;
    
    // 최종 낙상 이벤트 조건: 매우 큰 충격 OR (충격 + 기울기)
    bool fall_event = extreme_impact || strong_impact_with_tilt;
    
    if (fall_event) {
        // 낙상 이벤트 감지! (한 번만 알림)
        result.fall_detected = true;
        result.direction = determine_fall_direction(roll_deg, pitch_deg);
        
        // 쿨다운 시작 (10초간 추가 감지 방지)
        last_fall_time = now_ms;
        
        // 방향 문자열 변환
        const char* dir_str[] = {
            "없음", "앞", "뒤", "좌", "우", 
            "앞-좌", "앞-우", "뒤-좌", "뒤-우"
        };
        
        // 감지 경로 확인
        const char* detection_path = "";
        if (extreme_impact) {
            detection_path = " [매우큰충격]";
        } else if (strong_impact_with_tilt) {
            detection_path = " [강한충격+매우큰기울기]";
        } else {
            detection_path = " [기타]";
        }
        
        ESP_LOGW(TAG, "🚨 낙상 이벤트 감지%s 🚨", detection_path);
        ESP_LOGW(TAG, "충격: 총가속도=%.3fg", total_accel);
        ESP_LOGW(TAG, "기울기: 총변화=%.1f°", total_angle_change);
        ESP_LOGW(TAG, "낙상 방향: %s (각도: %.1f°)", dir_str[result.direction], result.fall_angle_deg);
        ESP_LOGW(TAG, "조건: 매우큰충격=%s(≥5.0g), 강한충격+매우큰기울기=%s(≥3.5g+70°)",
                 extreme_impact ? "✓" : "✗",
                 strong_impact_with_tilt ? "✓" : "✗");
        ESP_LOGW(TAG, "⚠️  알림 전송 후 5초간 재감지 방지 ⚠️");
        ESP_LOGW(TAG, "=======================================");
    }
    
    return result;
}

/**
 * @brief 낙상 감지 쿨다운 리셋 (수동으로 재감지 활성화)
 * @param ctx 컨텍스트
 */
void step_fall_reset_fall(step_fall_ctx_t* ctx) {
    if (ctx == NULL) return;
    
    ctx->fall_detected = false;
    ctx->fall_reset_time_ms = 0;
    
    // 쿨다운 해제 (즉시 재감지 가능)
    last_fall_time = 0;
    
    ESP_LOGI(TAG, "낙상 감지 쿨다운 수동 리셋 - 즉시 재감지 가능");
}
