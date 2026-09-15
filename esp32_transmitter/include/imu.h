#ifndef IMU_H
#define IMU_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/spi_master.h"

//no need to initialize spi again, because the spi is already initialized by the lcd
//we just need to read and write to the imu registers.

void init_imu(void); //need to set up the SPI bus for the SPI, also the cs select pin for the IMU. 
uint8_t imu_read_reg(uint8_t reg);
void imu_write_reg(uint8_t reg, uint8_t val);
void imu_read_raw(int16_t *gx, int16_t *gy, int16_t *gz,
                int16_t *ax, int16_t *ay, int16_t *az);
void imu_process_tilt(int16_t raw_ax, int16_t raw_ay, int16_t *out_x, int16_t *out_y);

//we just need to attach the imu to the spi bus by doing 
//spi_bus_add_device(spi number, &var, &spi)
#endif 