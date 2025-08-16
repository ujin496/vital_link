#ifndef SNTP_HELPER_H
#define SNTP_HELPER_H

#include "esp_err.h"
#include <time.h>

// SNTP 초기화 및 시간 동기화
esp_err_t sntp_init_and_sync(void);

// 현재 세계 시간 가져오기 (Unix timestamp)
time_t get_current_world_time(void);

// ESP 동작 시간과 세계 시간을 합친 타임스탬프 가져오기
int64_t get_combined_timestamp(void);

// 시간 포맷팅 (디버깅용)
void print_current_time(void);

// SNTP 동기화 상태 확인
int is_sntp_synced(void);

#endif // SNTP_HELPER_H
