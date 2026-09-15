#ifndef IMU_H
#define IMU_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/spi_master.h"

//no need to initialize spi again, because the spi is already initialized by the lcd
//we just need to read and write to the imu registers.

void imu_init(void); //need to set up the SPI bus for the SPI, also the cs select pin for the IMU. 
uint8_t imu_read_reg(uint8_t reg);
void imu_write_reg(uint8_t reg, uint8_t val);

//we just need to attach the imu to the spi bus by doing 
//spi_bus_add_device(spi number, &var, &spi)
#endif 