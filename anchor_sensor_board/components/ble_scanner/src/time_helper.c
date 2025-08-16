#include "time_helper.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "TIME_HELPER";

void print_current_time(void) {
    time_t now;
    struct tm timeinfo;
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    ESP_LOGI(TAG, "현재 시간: %s (UTC+9)", time_str);
}

void set_korea_timezone(void) {
    setenv("TZ", "KST-9", 1);
    tzset();
}

// 통합 타임스탬프 함수 (기본적으로 ESP 타이머 사용)
int64_t get_combined_timestamp(void) {
    // 기본적으로 ESP 타이머만 사용
    int64_t esp_time_ms = esp_timer_get_time() / 1000;
    ESP_LOGD(TAG, "Using ESP timer timestamp: %lldms", esp_time_ms);
    return esp_time_ms;
}
