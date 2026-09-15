#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "stdlib.h"
#include "xtensa/hal.h"
#include "imu.h"


static spi_handle_t imu_spi;
#define GPIO_CS_IMU 47 //while talking to this imu, the spi pulls this low.
#define IMU_HOST SPI2_HOST


void imu_init(void) {
    //we need to initialize the spi or configure the same thing as the lcd
    spi_device_interface_config_t devcfg = { //createwa a confifguration struct, specifying, how the spi
        //hardware should communicate with the SPI.
        .clock_speed_hz = 1*1000*1000, //1Mhz clock for the IMU
        .mode = 0, //initialize the spi mode to 0, this is so that every rising edge, our data can be sampled.
        .spics_io_num = GPIO_CS_IMU,
        .queue_size = 1, //one transaction at a time.
    };
    //check if we got any errors when we attach the imu to the spi bus.
    ESP_ERROR_CHECK(spi_bus_add_device(IMU_HOST, &devcfg, &imu_spi));
    
    //so initialize the accelerometer
    //0x10 is the CRT1_XL register, which controls the accelerometer and gyroscope 
    imu_write_reg(0x10, 0x48); 
    //i want to choose 104 hz which is the normal mode this is ODR_XL[3:0]
    //full scale range
    //00 good for tilts/standard motion
    //10 good for high vibration so prob this one
    //so 0100_1000 = 0x48
    //ODR_XL3, ODR_XL2, ODR_XL1, ODR_XL0, FS_XL1, FS_XL0, LPF1_BW_SEL, BW0_XL
    

    //we need to initialize the gyroscope using this register CTRL2_G
    //located at 0x11
    imu_write_reg(0x11, 0x44); //this is the third bit in that reigster located at 0x1C
    //ODR_G3, ODR_G2, ODR_G1, ODR_G0, FS_G1, FS_G0, FS_125, 0 
    //note the lsb must be set to low for the correction function of the gyroscope

    //normal mode for the first 4 bits
    //0100_0100 = 0x44
    //0100 for normal mode (higher bits), then next two 01 for 500 degrees per second.
     



}

void imu_write_reg(uint8_t reg, uint8_t val) {
    uint8_t tx_data[2] = {
        reg & 0x7F, val //bit 7 for write.
    }; //transmit the value in the register.

    spi_transaction_t t = {
        .length = 16, //2 bytes 
        .tx_buffer = tx_data,
    };

    spi_device_polling_transmit(imu_spi, &t);
}



uint8_t imu_read_reg(uint8_t reg) {
    uint8_t tx_data[2] = {
        reg | 0x80, 0x00 //bit 7 is a 1 for a read
    };
    uint8_t rx_data[2] = {0};
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };
    spi_device_polling_transmit(imu_spi, &t);

    return rx_data[1]; //return the first byte.

}
