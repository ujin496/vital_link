#ifndef TEMP_HUMID_SENSOR_H
#define TEMP_HUMID_SENSOR_H

#include <stdbool.h>

void temp_humid_sensor_init(void);
void temp_humid_log_task(void *pvParameters);
float get_temperature(void);
float get_humidity(void);
bool read_temp_humid_data(float *temperature, float *humidity);

// 재시도 로직이 포함된 내부 함수들
float get_temperature_with_retry(int max_retries);
float get_humidity_with_retry(int max_retries);

#endif // TEMP_HUMID_SENSOR_H