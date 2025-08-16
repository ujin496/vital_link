#ifndef I2C_HELPER_H
#define I2C_HELPER_H

#include "driver/i2c.h"

#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_FREQ_HZ 400000

void i2c_master_init(void);

#endif
