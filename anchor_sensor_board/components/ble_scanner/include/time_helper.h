#ifndef TIME_HELPER_H
#define TIME_HELPER_H

#include "esp_err.h"
#include <time.h>

// 공통 시간 포맷팅 함수
void print_current_time(void);

// 시간대 설정
void set_korea_timezone(void);

// 통합 타임스탬프 함수 (SNTP 또는 HTTP 중 하나 사용)
int64_t get_combined_timestamp(void);

#endif // TIME_HELPER_H
