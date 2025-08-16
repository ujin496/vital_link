#pragma once

#include "esp_err.h"
#include "driver/i2c.h"

// 초기화 함수 추가
esp_err_t mlx90614_init(i2c_port_t port);

// 기존 온도 읽기 함수
esp_err_t mlx90614_read_temp(float *object_temp);
