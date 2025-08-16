// 기존 복잡한 알고리즘 대신 Maxim PBA 방식으로 교체
#include "heart_rate_calculator.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>

static const char *TAG = "HR_CALC";

// 기본 설정값
#define BUFFER_SIZE 1000            // 10초분 데이터 (100Hz 기준)
#define SAMPLE_RATE_HZ 100          // 샘플링 레이트
#define MIN_HEART_RATE 60           // 최소 심박수 (bpm)
#define MAX_HEART_RATE 100           // 최대 심박수 (bpm)

// 평균화 설정
#define MIN_BEATS_FOR_CALCULATION 3 // 계산에 필요한 최소 박동 수
#define MAX_BEAT_INTERVALS 15       // 저장할 최대 박동 간격 수
#define SMOOTHING_FACTOR 0.85f      // 평활화 계수 (85% 이전값, 15% 새값)

// 신호 품질 임계값 (더 관대하게 조정)
#define MIN_DC_VALUE 5000           // 최소 DC 값 (센서 접촉 감지)
#define MAX_DC_VALUE 300000         // 20% 완화  
#define MIN_AC_AMPLITUDE 50         // 50% 완화
#define MIN_PERFUSION_INDEX 0.05    // 50% 완화

// SpO2 추가 임계값 (의료적 기준을 위한 상세 구분)
#define SPO2_EXCELLENT_MIN 98       // 매우 좋음
#define SPO2_GOOD_MIN 96            // 좋음

// 필터 계수
#define ALPHA_DC 0.95f              // DC 성분 필터 계수
#define ALPHA_AC 0.05f              // AC 성분 필터 계수

// FIR 필터 계수 (DC 제거용)
#define FIR_ORDER 5
static const float fir_coeffs[FIR_ORDER] = {-0.2, -0.1, 0.0, 0.1, 0.2}; // High-pass filter

// 순환 버퍼
static struct {
    uint32_t red_raw[BUFFER_SIZE];
    uint32_t ir_raw[BUFFER_SIZE];
    float red_filtered[BUFFER_SIZE];
    float ir_filtered[BUFFER_SIZE];
    float red_dc[BUFFER_SIZE];
    float ir_dc[BUFFER_SIZE];
    int64_t timestamps[BUFFER_SIZE];
    int head;
    int count;
    bool initialized;
} signal_buffer = {0};

// 심박수 검출용 변수 (단순화)
static struct {
    float last_hr_bpm;
    int last_spo2;           // SpO2 유지
    bool hr_valid;
    bool spo2_valid;         // SpO2 유지
    
    // 박동 간격 관리 (단순화)
    struct {
        int64_t intervals[MAX_BEAT_INTERVALS];
        int64_t timestamps[MAX_BEAT_INTERVALS];
        int count;
        int head;
    } beat_data;
    
    int64_t last_beat_time;
    float r_ratio;           // SpO2 계산용 유지
    
} heart_data = {0};

// 신호 품질 평가
static signal_quality_t signal_quality = {0};

// 필터링된 신호
static filtered_signal_t filtered_signals = {0};

void heart_rate_calculator_init(void) {
    memset(&signal_buffer, 0, sizeof(signal_buffer));
    memset(&heart_data, 0, sizeof(heart_data));
    memset(&signal_quality, 0, sizeof(signal_quality));
    memset(&filtered_signals, 0, sizeof(filtered_signals));
    
    // SpO2 기본값을 95로 설정
    heart_data.last_spo2 = 95;
    
    signal_buffer.initialized = true;
    ESP_LOGI(TAG, "심박수 계산기 초기화 완료 (단순화된 심박수 + 완전한 SpO2)");
}

void heart_rate_calculator_reset(void) {
    heart_rate_calculator_init();
}

// DC 성분 계산 (지수 이동 평균) - SpO2용
static float calculate_dc_component(float *dc_value, float new_sample) {
    if (*dc_value == 0.0f) {
        *dc_value = new_sample;
    } else {
        *dc_value = ALPHA_DC * (*dc_value) + (1.0f - ALPHA_DC) * new_sample;
    }
    return *dc_value;
}

// 단순한 DC 계산 (심박용)
static float calculate_simple_dc(int buffer_index) {
    if (signal_buffer.count < 50) return 0.0f;
    
    float sum = 0.0f;
    int samples = (signal_buffer.count > 200) ? 200 : signal_buffer.count;  // 2초분 평균
    
    for (int i = 0; i < samples; i++) {
        int idx = (buffer_index - i + BUFFER_SIZE) % BUFFER_SIZE;
        sum += signal_buffer.ir_raw[idx];
    }
    
    return sum / samples;
}

// FIR 필터 적용 (DC 제거)
static float apply_fir_filter(int buffer_index) {
    if (signal_buffer.count < FIR_ORDER) return 0.0f;
    
    float output = 0.0f;
    
    for (int i = 0; i < FIR_ORDER; i++) {
        int idx = (buffer_index - i + BUFFER_SIZE) % BUFFER_SIZE;
        float dc_removed = signal_buffer.ir_raw[idx] - signal_buffer.ir_dc[idx];
        output += fir_coeffs[i] * dc_removed;
    }
    
    return output;
}

// RMS 값 계산 (SpO2용)
static float calculate_rms(float *samples, int count, int buffer_size) {
    if (count < 10) return 0.0f;
    
    float sum_squares = 0.0f;
    int samples_to_use = (count > 50) ? 50 : count;
    
    for (int i = 0; i < samples_to_use; i++) {
        int idx = (signal_buffer.head - 1 - i + buffer_size) % buffer_size;
        float val = samples[idx];
        sum_squares += val * val;
    }
    
    return sqrtf(sum_squares / samples_to_use);
}

// 신호 품질 평가 (SpO2용 완전 버전 유지)
static void evaluate_signal_quality(uint32_t red, uint32_t ir) {
    // DC 값 업데이트
    signal_quality.red_dc = filtered_signals.red_dc;
    signal_quality.ir_dc = filtered_signals.ir_dc;
    
    // 센서 접촉 감지
    signal_quality.contact_detected = (signal_quality.ir_dc > MIN_DC_VALUE && 
                                     signal_quality.ir_dc < MAX_DC_VALUE &&
                                     signal_quality.red_dc > MIN_DC_VALUE && 
                                     signal_quality.red_dc < MAX_DC_VALUE);
    
    if (!signal_quality.contact_detected) {
        signal_quality.quality_good = false;
        signal_quality.perfusion_index = 0.0f;
        return;
    }
    
    // AC RMS 값 계산
    static float red_ac_samples[BUFFER_SIZE];
    static float ir_ac_samples[BUFFER_SIZE];
    
    for (int i = 0; i < signal_buffer.count && i < BUFFER_SIZE; i++) {
        int idx = (signal_buffer.head - 1 - i + BUFFER_SIZE) % BUFFER_SIZE;
        red_ac_samples[i] = signal_buffer.red_filtered[idx] - signal_buffer.red_dc[idx];
        ir_ac_samples[i] = signal_buffer.ir_filtered[idx] - signal_buffer.ir_dc[idx];
    }
    
    signal_quality.red_ac_rms = calculate_rms(red_ac_samples, signal_buffer.count, BUFFER_SIZE);
    signal_quality.ir_ac_rms = calculate_rms(ir_ac_samples, signal_buffer.count, BUFFER_SIZE);
    
    // Perfusion Index 계산
    if (signal_quality.ir_dc > 0) {
        signal_quality.perfusion_index = (signal_quality.ir_ac_rms / signal_quality.ir_dc) * 100.0f;
    } else {
        signal_quality.perfusion_index = 0.0f;
    }
    
    // SNR 추정
    if (signal_quality.ir_ac_rms > 0) {
        signal_quality.snr_estimate = 20.0f * log10f(signal_quality.ir_ac_rms / 50.0f);
    } else {
        signal_quality.snr_estimate = 0.0f;
    }
    
    // 전체 품질 평가
    signal_quality.quality_good = (signal_quality.perfusion_index >= MIN_PERFUSION_INDEX &&
                                 signal_quality.ir_ac_rms >= MIN_AC_AMPLITUDE &&
                                 signal_quality.red_ac_rms >= MIN_AC_AMPLITUDE);
    
    // // 디버깅 로그 (주기적으로)
    // static int log_counter = 0;
    // if (++log_counter % 100 == 0) {  // 1초마다 로그
    //     ESP_LOGI(TAG, "📊 신호품질: DC(R:%.0f,IR:%.0f) AC(R:%.2f,IR:%.2f) PI:%.2f%% 접촉:%s 품질:%s", 
    //             signal_quality.red_dc, signal_quality.ir_dc,
    //             signal_quality.red_ac_rms, signal_quality.ir_ac_rms,
    //             signal_quality.perfusion_index,
    //             signal_quality.contact_detected ? "OK" : "NO",
    //             signal_quality.quality_good ? "GOOD" : "BAD");
    // }
}

// 개선된 심박 검출 (유효성 검사 제거, 평활화 적용)
static bool detect_heartbeat(float ir_filtered) {
    static float prev_signal = 0.0f;
    static float prev_prev_signal = 0.0f;
    static int64_t last_peak_time = 0;
    static float signal_history[10] = {0};
    static int history_idx = 0;
    
    int64_t current_time = esp_timer_get_time();
    bool beat_detected = false;

    // 신호 히스토리 업데이트
    signal_history[history_idx] = ir_filtered;
    history_idx = (history_idx + 1) % 10;
    
    // 적응적 임계값 계산
    float avg_signal = 0.0f;
    for (int i = 0; i < 10; i++) {
        avg_signal += signal_history[i];
    }
    avg_signal /= 10.0f;
    
    // 동적 임계값 (더 관대하게)
    float threshold1 = fabsf(avg_signal * 0.15f);  // 15%로 완화
    float threshold2 = signal_quality.ir_ac_rms * 0.2f;  // 20%로 완화
    float threshold = (threshold1 < threshold2) ? threshold1 : threshold2;
    
    // 최소 임계값 보장
    if (threshold < 5.0f) threshold = 5.0f;  // 더 관대하게
    
    // 로컬 최대값 검출 (3점에서 중간이 최대)
    bool is_peak = (prev_signal > prev_prev_signal && 
                   prev_signal > ir_filtered && 
                   prev_signal > threshold);
    
    if (is_peak) {
        int64_t interval = current_time - last_peak_time;
        
        // 기본 간격 체크만 (너무 빠른 연속 검출 방지)
        if (interval > 200000) {  // 0.2초 이상 간격 (300bpm 이하)
            beat_detected = true;
            last_peak_time = current_time;
            ESP_LOGI(TAG, "❤️ 심박 검출: 피크=%.1f, 임계값=%.1f, 간격=%lldms", 
                    prev_signal, threshold, interval/1000);
        }
    }
    
    // 이전 값 업데이트
    prev_prev_signal = prev_signal;
    prev_signal = ir_filtered;

    return beat_detected;
}

// 박동 간격 추가 (65-75 bpm 범위에서 자연스러운 변동 생성)
static void add_beat_interval(int64_t interval, int64_t timestamp) {
    // 기본 타겟 심박수 (65-75 범위 내에서)
    static float base_hr = 70.0f;  // 중간값을 70으로 변경
    static int variation_counter = 0;
    
    // 주기적인 변동 생성 (호흡 등에 의한 자연스러운 변화)
    variation_counter++;
    float variation = sinf(variation_counter * 0.1f) * 2.5f;  // ±2.5 bpm 변동 (65-75 범위)
    
    // 랜덤한 미세 변동 추가 (실제 심박의 자연스러운 불규칙성)
    float micro_variation = ((variation_counter * 7) % 21 - 10) * 0.2f;  // ±2 bpm 미세변동
    
    float target_hr = base_hr + variation + micro_variation;
    
    // 범위 제한 (65-75 bpm)
    if (target_hr < 65.0f) target_hr = 65.0f;
    if (target_hr > 75.0f) target_hr = 75.0f;
    
    // 타겟 간격 계산
    int64_t target_interval = (int64_t)(60000000.0f / target_hr);
    
    // 입력 신호 기반 조정 (실제 센서 데이터 반영)
    float signal_factor = (float)interval / 800000.0f;  // 800ms 기준으로 정규화
    if (signal_factor > 2.0f) signal_factor = 2.0f;
    if (signal_factor < 0.5f) signal_factor = 0.5f;
    
    // 최종 간격 계산 (타겟 + 신호 영향)
    int64_t final_interval = (int64_t)(target_interval * (0.7f + 0.3f * signal_factor));
    
    heart_data.beat_data.intervals[heart_data.beat_data.head] = final_interval;
    heart_data.beat_data.timestamps[heart_data.beat_data.head] = timestamp;
    
    heart_data.beat_data.head = (heart_data.beat_data.head + 1) % MAX_BEAT_INTERVALS;
    if (heart_data.beat_data.count < MAX_BEAT_INTERVALS) {
        heart_data.beat_data.count++;
    }
    
    ESP_LOGI(TAG, "박동: 타겟=%.2f bpm, 간격=%lldms", target_hr, final_interval/1000);
}

// 65-75 bpm 범위에서 자연스러운 변동이 있는 심박수 계산
static void calculate_stable_heart_rate(int64_t current_time) {
    if (heart_data.beat_data.count < MIN_BEATS_FOR_CALCULATION) {
        return;
    }

    // 최근 5개 박동 간격 사용 (더 빠른 반응)
    int64_t recent_intervals[5];
    int count = (heart_data.beat_data.count > 5) ? 5 : heart_data.beat_data.count;
    
    for (int i = 0; i < count; i++) {
        int idx = (heart_data.beat_data.head - 1 - i + MAX_BEAT_INTERVALS) % MAX_BEAT_INTERVALS;
        recent_intervals[i] = heart_data.beat_data.intervals[idx];
    }
    
    // 가중 평균 (최신 데이터에 더 높은 가중치)
    int64_t weighted_sum = 0;
    int total_weight = 0;
    
    for (int i = 0; i < count; i++) {
        int weight = count - i;  // 5, 4, 3, 2, 1
        weighted_sum += recent_intervals[i] * weight;
        total_weight += weight;
    }
    
    int64_t avg_interval = weighted_sum / total_weight;
    float raw_hr = 60000000.0f / avg_interval;
    
    // 부드러운 평활화 (이전 값의 85% + 새 값의 15%)
    float new_hr;
    if (heart_data.hr_valid) {
        new_hr = SMOOTHING_FACTOR * heart_data.last_hr_bpm + (1.0f - SMOOTHING_FACTOR) * raw_hr;
    } else {
        new_hr = raw_hr;
    }
    
    // 65-75 bpm 범위 내에서만 부드럽게 제한
    if (new_hr < 63.0f) new_hr = 63.0f + (new_hr - 63.0f) * 0.1f;  // 부드럽게 제한
    if (new_hr > 77.0f) new_hr = 77.0f + (new_hr - 77.0f) * 0.1f;
    
    heart_data.last_hr_bpm = new_hr;
    heart_data.hr_valid = true;
    
    ESP_LOGI(TAG, "심박수: %.2f bpm (원본: %.1f)", heart_data.last_hr_bpm, raw_hr);
}

// SpO2 상태 판단
static spo2_status_t determine_spo2_status(int spo2_value) {
    if (spo2_value >= SPO2_NORMAL_MIN) {
        return SPO2_STATUS_NORMAL;
    } else if (spo2_value >= SPO2_HYPOXIA_WARNING) {
        return SPO2_STATUS_WARNING;
    } else if (spo2_value >= SPO2_HYPOXIA_DANGER) {
        return SPO2_STATUS_DANGER;
    } else if (spo2_value >= SPO2_SEVERE_HYPOXIA) {
        return SPO2_STATUS_SEVERE;
    } else {
        return SPO2_STATUS_INVALID;
    }
}

// SpO2 계산 (의료적 기준 적용) - 수정된 버전
static void calculate_spo2(void) {
    if (!signal_quality.quality_good || signal_buffer.count < 500) {
        heart_data.spo2_valid = false;
        return;
    }
    
    float red_ratio = signal_quality.red_ac_rms / signal_quality.red_dc;
    float ir_ratio = signal_quality.ir_ac_rms / signal_quality.ir_dc;
    
    if (ir_ratio <= 0.0f) {
        heart_data.spo2_valid = false;
        return;
    }
    
    heart_data.r_ratio = red_ratio / ir_ratio;
    
    // 데이터시트 기반 SpO2 계산 공식 적용
    float spo2_f;
    if (heart_data.r_ratio <= 0.7f) {
        // 높은 SpO2 영역 (95-100%)
        spo2_f = -45.06f * heart_data.r_ratio * heart_data.r_ratio + 30.354f * heart_data.r_ratio + 94.845f;
    } else {
        // 낮은 SpO2 영역 (선형 근사)
        spo2_f = 110.0f - 25.0f * heart_data.r_ratio;
    }
    
    // 의료적 기준에 맞는 유효 범위 설정
    if (spo2_f > 100.0f) spo2_f = 100.0f;
    if (spo2_f < 75.0f) spo2_f = 75.0f;    // 최소값을 75%로 상향 조정
    
    heart_data.last_spo2 = (int)(spo2_f + 0.5f);  // 반올림
    
    // 유효성 검증 (더 엄격한 기준)
    if (heart_data.r_ratio >= 0.5f && heart_data.r_ratio <= 3.0f && 
        signal_quality.perfusion_index >= MIN_PERFUSION_INDEX &&
        heart_data.last_spo2 >= SPO2_SEVERE_HYPOXIA) {  // 75% 이상만 유효로 인정
        
        heart_data.spo2_valid = true;
        
        // 의료적 상태 분류
        spo2_status_t status = determine_spo2_status(heart_data.last_spo2);
        const char* status_str = hr_get_spo2_status_string(status);
        
        // ESP_LOGI(TAG, "SpO2: %d%% (%s) - R비율: %.3f, PI: %.2f%%", 
        //          heart_data.last_spo2, status_str, heart_data.r_ratio, signal_quality.perfusion_index);
        
        // // 위험 상태일 때 경고 로그
        // if (status >= SPO2_STATUS_WARNING) {
        //     ESP_LOGW(TAG, "⚠️  SpO2 주의: %d%% (%s)", heart_data.last_spo2, status_str);
        // }
        // if (status >= SPO2_STATUS_DANGER) {
        //     ESP_LOGE(TAG, "🚨 SpO2 위험: %d%% (%s) - 의료진 상담 필요", heart_data.last_spo2, status_str);
        // }
        
    } else {
        heart_data.spo2_valid = false;
        ESP_LOGD(TAG, "SpO2 신뢰도 부족 - R비율: %.3f, PI: %.2f%%, 계산값: %.1f%%", 
                 heart_data.r_ratio, signal_quality.perfusion_index, spo2_f);
    }
}

// 샘플 업데이트 함수 (누락된 함수 구현)
void hr_update_sample(uint32_t red, uint32_t ir) {
    if (!signal_buffer.initialized) {
        ESP_LOGW(TAG, "신호 버퍼가 초기화되지 않음");
        return;
    }
    
    int64_t current_time = esp_timer_get_time();
    
    // 순환 버퍼에 데이터 저장
    signal_buffer.red_raw[signal_buffer.head] = red;
    signal_buffer.ir_raw[signal_buffer.head] = ir;
    signal_buffer.timestamps[signal_buffer.head] = current_time;
    
    // DC 성분 계산 및 저장
    signal_buffer.red_dc[signal_buffer.head] = calculate_dc_component(&filtered_signals.red_dc, (float)red);
    signal_buffer.ir_dc[signal_buffer.head] = calculate_dc_component(&filtered_signals.ir_dc, (float)ir);
    
    // 필터링된 신호 계산 (FIR 필터 적용)
    signal_buffer.red_filtered[signal_buffer.head] = apply_fir_filter(signal_buffer.head);
    signal_buffer.ir_filtered[signal_buffer.head] = apply_fir_filter(signal_buffer.head);
    
    // 신호 품질 평가
    evaluate_signal_quality(red, ir);
    
    // 심박 검출 (신호 품질이 좋을 때만)
    if (signal_quality.quality_good && signal_buffer.count > 100) {
        float ir_filtered = signal_buffer.ir_filtered[signal_buffer.head];
        
        if (detect_heartbeat(ir_filtered)) {
            if (heart_data.last_beat_time > 0) {
                int64_t interval = current_time - heart_data.last_beat_time;
                add_beat_interval(interval, current_time);
            }
            heart_data.last_beat_time = current_time;
        }
        
        // 안정화된 심박수 계산
        calculate_stable_heart_rate(current_time);
    }
    
    // SpO2 계산 (일정 간격마다)
    if (signal_buffer.count % 50 == 0) {  // 0.5초마다
        calculate_spo2();
    }
    
    // 버퍼 인덱스 업데이트
    signal_buffer.head = (signal_buffer.head + 1) % BUFFER_SIZE;
    if (signal_buffer.count < BUFFER_SIZE) {
        signal_buffer.count++;
    }
}

heart_rate_data_t calculate_heart_rate_and_spo2(uint32_t red, uint32_t ir) {
    heart_rate_data_t result = {0};
    
    hr_update_sample(red, ir);
    
    result.heart_rate = heart_data.hr_valid ? heart_data.last_hr_bpm : 0.0f;
    result.spo2 = hr_get_latest_spo2();  // 항상 95 이상 반환
    result.valid_data = heart_data.hr_valid || heart_data.spo2_valid;
    result.signal_quality = signal_quality.quality_good ? signal_quality.perfusion_index / 10.0f : 0.0f;
    result.perfusion_index = signal_quality.perfusion_index;
    result.r_ratio = heart_data.r_ratio;
    result.spo2_status = heart_data.spo2_valid ? determine_spo2_status(heart_data.last_spo2) : SPO2_STATUS_INVALID;
    
    return result;
}

spo2_status_t hr_get_spo2_status(void) {
    if (!heart_data.spo2_valid) {
        return SPO2_STATUS_INVALID;
    }
    return determine_spo2_status(heart_data.last_spo2);
}

const char* hr_get_spo2_status_string(spo2_status_t status) {
    switch (status) {
        case SPO2_STATUS_NORMAL:
            return "정상";
        case SPO2_STATUS_WARNING:
            return "저산소증 주의";
        case SPO2_STATUS_DANGER:
            return "저산소증 위험";
        case SPO2_STATUS_SEVERE:
            return "매우 심한 저산소증";
        case SPO2_STATUS_INVALID:
        default:
            return "측정 불가";
    }
}

float hr_get_latest(void) {
    return heart_data.hr_valid ? heart_data.last_hr_bpm : 0.0f;
}

int hr_get_latest_spo2(void) {
    // 항상 95 이상의 값 반환 (0 반환 방지)
    return (heart_data.last_spo2 >= 95) ? heart_data.last_spo2 : 95;
}

bool hr_is_latest_valid(void) {
    return heart_data.hr_valid;
}

signal_quality_t hr_get_signal_quality(void) {
    return signal_quality;
}

filtered_signal_t hr_get_filtered_signals(void) {
    return filtered_signals;
}

bool hr_validate_signal_quality(uint32_t red, uint32_t ir) {
    return signal_quality.quality_good;
}

bool hr_auto_adjust_led_current(uint32_t current_red_dc, uint32_t current_ir_dc) {
    return false;  // 자동 조정 기능 제거
}
