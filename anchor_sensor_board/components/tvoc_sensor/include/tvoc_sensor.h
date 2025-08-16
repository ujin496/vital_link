#ifndef TVOC_SENSOR_H
#define TVOC_SENSOR_H

#include <stdbool.h>

void tvoc_sensor_init(void);
void tvoc_log_task(void *pvParameters);

float mq135_get_rs(void);
float mq135_get_ratio(float rs);
float mq135_get_tvoc_ppb(float ratio);
bool mq135_detect_gas(void);

#endif // TVOC_SENSOR_H
