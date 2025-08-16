#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include "esp_err.h"
#include "driver/adc.h"   // ✅ 레거시 ADC 드라이버 사용

#ifdef __cplusplus
extern "C" {
#endif

// 레거시 드라이버 타입은 adc1_channel_t 입니다.
esp_err_t light_sensor_init(adc1_channel_t channel);
esp_err_t light_sensor_read_raw(int *raw_value);
esp_err_t light_sensor_read_lux(float *lux_value);

#ifdef __cplusplus
}
#endif
#endif // LIGHT_SENSOR_H
