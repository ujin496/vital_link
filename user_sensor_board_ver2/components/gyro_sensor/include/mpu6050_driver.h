#pragma once

#include "esp_err.h"
#include "driver/i2c.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t gx;
    int16_t gy;
    int16_t gz;
} mpu6050_data_t;

/**
 * @brief MPU6050 초기화 (Power management 및 설정)
 */
esp_err_t mpu6050_init(i2c_port_t port);

/**
 * @brief 가속도 + 자이로 데이터를 읽어서 구조체에 저장
 */
esp_err_t mpu6050_read_data(i2c_port_t port, mpu6050_data_t *out_data);

#ifdef __cplusplus
}
#endif
