#include "sntp_helper.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <string.h>
#include <time.h>

// lwip의 SNTP 사용
#include "lwip/apps/sntp.h"
#include "time_helper.h"

static const char *TAG = "SNTP_HELPER";

// SNTP 동기화 완료 플래그
static bool sntp_synced = false;

// ESP 시작 시간 (마이크로초)
static int64_t esp_start_time_us = 0;

// SNTP 초기화 상태
static bool sntp_initialized = false;

// SNTP 동기화 상태 확인 함수
static bool check_sntp_sync_status(void) {
    time_t now;
    time(&now);
    
    ESP_LOGI(TAG, "현재 시간 확인: %ld (epoch)", now);
    
    // 2020년 이후 시간이면 동기화된 것으로 간주
    if (now > 1577836800) {  // 2020-01-01 00:00:00 UTC
        return true;
    }
    
    // 시간 구조체로 변환하여 더 자세한 정보 출력
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    ESP_LOGI(TAG, "현재 시간: %04d-%02d-%02d %02d:%02d:%02d", 
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    
    return false;
}

esp_err_t sntp_init_and_sync(void) {
    ESP_LOGI(TAG, "SNTP 초기화 시작");
    
    // ESP 시작 시간 기록
    esp_start_time_us = esp_timer_get_time();
    
    // SNTP가 이미 초기화되었는지 확인
    if (sntp_initialized) {
        ESP_LOGW(TAG, "SNTP가 이미 초기화됨");
        return ESP_OK;
    }
    
    // 초기 시간 상태 확인
    ESP_LOGI(TAG, "초기 시간 상태 확인:");
    check_sntp_sync_status();
    
    // SNTP 설정 (lwip 방식)
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    
    // 더 안정적인 NTP 서버들 사용 (한국 서버 포함)
    // sntp_setservername(0, "kr.pool.ntp.org");      // 한국 NTP 서버
    // sntp_setservername(1, "time.google.com");      // Google 시간 서버
    // sntp_setservername(2, "pool.ntp.org");         // 글로벌 NTP 서버
    sntp_setservername(0, "time.windows.com");     // Windows 시간 서버
    
    ESP_LOGI(TAG, "SNTP 서버 설정 완료");
    // ESP_LOGI(TAG, "서버 0: kr.pool.ntp.org");
    // ESP_LOGI(TAG, "서버 1: time.google.com");
    // ESP_LOGI(TAG, "서버 2: pool.ntp.org");
    ESP_LOGI(TAG, "서버 0: time.windows.com");
    
    // SNTP 시작
    sntp_init();
    sntp_initialized = true;
    
    ESP_LOGI(TAG, "SNTP 초기화 완료, 시간 동기화 대기 중...");
    
    // SNTP 동기화 대기 (최대 90초로 증가)
    int retry_count = 0;
    while (!sntp_synced && retry_count < 90) {
        ESP_LOGI(TAG, "SNTP 동기화 대기 중... (%d/90)", retry_count + 1);
        
        // SNTP 동기화 상태 확인
        if (check_sntp_sync_status()) {
            sntp_synced = true;
            ESP_LOGI(TAG, "SNTP 동기화 성공!");
            break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
        retry_count++;
    }
    
    if (sntp_synced) {
        ESP_LOGI(TAG, "SNTP 시간 동기화 성공");
        print_current_time();
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "SNTP 시간 동기화 실패");
        return ESP_ERR_TIMEOUT;
    }
}

// SNTP 전용 함수들 (HTTP와 구분)
time_t sntp_get_current_world_time(void) {
    time_t now;
    time(&now);
    return now;
}

// 헤더에서 선언된 함수명과 일치하도록 추가
time_t get_current_world_time(void) {
    time_t now;
    time(&now);
    return now;
}

int64_t sntp_get_combined_timestamp(void) {
    if (sntp_synced) {
        // SNTP 동기화된 경우: 순수한 유닉스 타임스탬프만 사용
        time_t world_time = sntp_get_current_world_time();
        int64_t unix_timestamp_ms = (int64_t)world_time * 1000;
        
        ESP_LOGD(TAG, "SNTP synced, using Unix timestamp: %ld -> %lldms", 
                 world_time, unix_timestamp_ms);
        
        return unix_timestamp_ms;
    } else {
        // SNTP 동기화되지 않은 경우: ESP 타이머만 사용
        int64_t esp_time_ms = esp_timer_get_time() / 1000;
        ESP_LOGW(TAG, "SNTP not synced, using ESP timer only: %lldms", esp_time_ms);
        return esp_time_ms;
    }
}

// SNTP 동기화 상태 확인
int is_sntp_synced(void) {
    return sntp_synced;
}
