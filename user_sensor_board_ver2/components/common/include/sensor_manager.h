#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 센서 매니저 태스크 시작
 * @return ESP_OK 성공, ESP_FAIL 실패
 */
esp_err_t sensor_manager_start(void);

/**
 * @brief 센서 매니저 태스크 중지
 */
void sensor_manager_stop(void);

/**
 * @brief I2C 버스 복구 함수
 * @return ESP_OK 성공, ESP_FAIL 실패
 */
esp_err_t i2c_bus_recover(void);

#ifdef __cplusplus
}
#endif

#endif // SENSOR_MANAGER_H
