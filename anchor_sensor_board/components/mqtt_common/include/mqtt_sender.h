#ifndef MQTT_SENDER_H
#define MQTT_SENDER_H

#include "sensor_data.h"

void mqtt_send_influx_sensor_data(const influx_sensor_data_t* data);

#endif
