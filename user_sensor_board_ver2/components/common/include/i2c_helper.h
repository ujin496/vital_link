#ifndef I2C_HELPER_H
#define I2C_HELPER_H

#include "driver/i2c.h"

// I2C0: 자이로 센서용 (MPU6050)
#define I2C_MASTER_NUM_0 I2C_NUM_0
#define I2C_MASTER_SDA_IO_0 21
#define I2C_MASTER_SCL_IO_0 22
#define I2C_MASTER_FREQ_HZ_0 400000

// I2C1: 심박/체온 센서용 (다른 GPIO 핀 사용)
#define I2C_MASTER_NUM_1 I2C_NUM_1
#define I2C_MASTER_SDA_IO_1 18  // GPIO 18로 변경
#define I2C_MASTER_SCL_IO_1 19  // GPIO 19로 변경
#define I2C_MASTER_FREQ_HZ_1 100000

void i2c_master_init(void);
esp_err_t i2c_bus_recover_0(void);
esp_err_t i2c_bus_recover_1(void);

#endif
