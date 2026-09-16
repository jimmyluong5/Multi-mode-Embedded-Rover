#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "stdlib.h"
#include "xtensa/hal.h"
#include "imu.h"


static spi_device_handle_t imu_spi;
#define GPIO_CS_IMU 47 //while talking to this imu, the spi pulls this low.
#define IMU_HOST SPI2_HOST
static const char *TAG = "IMU";

void init_imu(void) {
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

    //next we need to initialize the auto-increment register for the increment, so each time
    //we receive multiple bytes of data we need to advance to the next register address.

    //this register is the CTRL3_C at 0x12
    imu_write_reg(0x12, 0x44);
    //BOOT, BDU, H_LACTIVE, PP_OD, SIM, IF_INC, BLE, SW_RESET
    //BOOT - 0 for normal
    //bdu - 1
    //h_lactive - 0
    //pp_od - 0
    //sim - 0
    //if_inc - 1
    //ble - 0
    //sw_reset - 0

    //0100_0100 - 0x44

    
}

void imu_write_reg(uint8_t reg, uint8_t val) {
    uint8_t tx_data[2] = { //contains 2 bytes, 1 byte for each index.
        reg & 0x7F, val //bit 7 for write.
    }; //transmit the value in the register.

    //reg & 0x7F bitwise AND, where 0x7F is 0111_1111, then the 2nd byte will be the value want to write to the register.
    //so we are guaranteed to have a 0 in the 7th bit, which means to write.
    //for the imu, to read it would be this 1111_1111

    //for write it would be 0111_1111
    //0 for write, 1 for read in the 7th bit., 7, 6, 5, 4, 3, 2, 1, 0

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
    //0x80 = 1000_0000, guaranteed to have a 1 in the 7th bit meaning to write.
    uint8_t rx_data[2] = {0};
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };
    spi_device_polling_transmit(imu_spi, &t);

    return rx_data[1]; //return the first byte.

}

//technically we don't need gx or gy, because we are not rotating about the x or y axis.

//and we can't remove them, because the auto-increment register cannot skip bytes
//so we just keep them.
void imu_read_raw(int16_t *gx, int16_t *gy, int16_t *gz, int16_t *ax, int16_t *ay, int16_t *az) {

    //create the transmitting and receiving buffers
    //the total spi transaction of bytes like receiving and transmitting is 13 bytes
    //1 byte just for commanding the spi that we want to read
    //
    /* Gyro X	OUTX_L_G (0x22)	OUTX_H_G (0x23)	2 bytes
    Gyro Y	OUTY_L_G (0x24)	OUTY_H_G (0x25)	2 bytes
    Gyro Z	OUTZ_L_G (0x26)	OUTZ_H_G (0x27)	2 bytes
    Accel X	OUTX_L_XL (0x28)	OUTX_H_XL (0x29)	2 bytes
    Accel Y	OUTY_L_XL (0x2A)	OUTY_H_XL (0x2B)	2 bytes
    Accel Z	OUTZ_L_XL (0x2C)	OUTZ_H_XL (0x2D)	2 bytes */
    uint8_t tx_data[13] = {0x22 | 0x80}; //logical OR, 
    //all the existing 1s surivve
    //first byte must be 1000_0000 so we want to read
    //0x22 - is the first register we want to read
    //0x80 - 1000_0000

    uint8_t rx_data[13] = {0x22 | 0x00}; //or 0, because we want to write into this buffer.

    spi_transaction_t t = {
        .length = 13*8, //13 bytes * 8 bits each byte = 104 bits.
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };
    spi_device_polling_transmit(imu_spi, &t); //place imu on spi bus.

    //rx_data[0] is dummy byte during address transmit. 
    // Gyroscope X, Y, Z (Registers 0x22 - 0x27)
    *gx = (int16_t)((rx_data[2]  << 8) | rx_data[1]);
    *gy = (int16_t)((rx_data[4]  << 8) | rx_data[3]);
    *gz = (int16_t)((rx_data[6]  << 8) | rx_data[5]);
    // Accelerometer X, Y, Z (Registers 0x28 - 0x2D)
    *ax = (int16_t)((rx_data[8]  << 8) | rx_data[7]);
    *ay = (int16_t)((rx_data[10] << 8) | rx_data[9]);
    *az = (int16_t)((rx_data[12] << 8) | rx_data[11]);

}

// Deadband threshold: ~7 degrees tilt (~1000 counts on +/-4g range)
#define IMU_DEADBAND    1000
#define GRID_CENTER_X   85
#define GRID_CENTER_Y   181
#define GRID_RADIUS_X   42
#define GRID_RADIUS_Y   39

static volatile int16_t s_latest_tilt_x = 0;
static volatile int16_t s_latest_tilt_y = 0;

void imu_process_tilt(int16_t raw_ax, int16_t raw_ay, int16_t *out_x, int16_t *out_y) {
    // 1. Orient directions:
    // Tilt right -> positive X (+steering right)
    // Tilt forward/down -> positive Y (+throttle forward)
    int16_t mapped_x = -raw_ax;
    int16_t mapped_y = -raw_ay;

    s_latest_tilt_x = mapped_x;
    s_latest_tilt_y = mapped_y;
    
    // 2. Deadband filter for X (Steering)
    if (abs(mapped_x) < IMU_DEADBAND) {
        *out_x = 0; // Lock to neutral zero
    } 
    
    else {
        *out_x = mapped_x;
    }

    // 3. Deadband filter for Y (Throttle)
    if (abs(mapped_y) < IMU_DEADBAND) {
        *out_y = 0; // Lock to neutral zero
    } 

    else {
        *out_y = mapped_y;
    }

    
}

void imu_get_screen_coords(int *out_x, int *out_y) {
    int16_t tilt_x = s_latest_tilt_x;
    int16_t tilt_y = s_latest_tilt_y;

    // Max tilt range (~35 degrees tilt = ~4500 counts)
    int pixel_x = GRID_CENTER_X + ((int)tilt_x * GRID_RADIUS_X) / 4500;
    int pixel_y = GRID_CENTER_Y - ((int)tilt_y * GRID_RADIUS_Y) / 4500; // Invert Y so tilt forward moves dot up

    // Clamp inside the grid box boundary
    if (pixel_x < GRID_CENTER_X - GRID_RADIUS_X) pixel_x = GRID_CENTER_X - GRID_RADIUS_X;
    if (pixel_x > GRID_CENTER_X + GRID_RADIUS_X) pixel_x = GRID_CENTER_X + GRID_RADIUS_X;
    if (pixel_y < GRID_CENTER_Y - GRID_RADIUS_Y) pixel_y = GRID_CENTER_Y - GRID_RADIUS_Y;
    if (pixel_y > GRID_CENTER_Y + GRID_RADIUS_Y) pixel_y = GRID_CENTER_Y + GRID_RADIUS_Y;

    if (out_x) *out_x = pixel_x;
    if (out_y) *out_y = pixel_y;
}

void imu_get_tilt_deg(int *pitch_deg, int *roll_deg) {
    if (pitch_deg) *pitch_deg = ((int)s_latest_tilt_y * 90) / 8192;
    if (roll_deg)  *roll_deg  = ((int)s_latest_tilt_x * 90) / 8192;
}





