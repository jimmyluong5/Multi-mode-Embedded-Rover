#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "pretty_effect.h"
#include "lcd.h"
#include "decode_image.h"
#include "transmit_data.h"

//this code is from the esp32 expressif github for this type of display 

/*
 This code displays some fancy graphics on the 320x240 LCD on an ESP-WROVER_KIT board.
 This example demonstrates the use of both spi_device_transmit as well as
 spi_device_queue_trans/spi_device_get_trans_result and pre-transmit callbacks.

 Some info about the ILI9341/ST7789V: It has an C/D line, which is connected to a GPIO here. It expects this
 line to be low for a command and high for data. We use a pre-transmit callback here to control that
 line: every transaction has as the user-definable argument the needed state of the D/C line and just
 before the transaction is sent, the callback will set this line to the correct state.
*/


//SCK - GPIO7
//SDI/MOSI - GPIO8
//SDO/MISO - GPIO9
//CS - GPIO 15
//DC - GPIO 16
//RESET - GPIO 17

#define LCD_HOST  SPI2_HOST

#define PIN_NUM_MISO 9
#define PIN_NUM_MOSI 8
#define PIN_NUM_CLK  7 //sck
#define PIN_NUM_CS   15

#define PIN_NUM_DC   16
#define PIN_NUM_RST  17
//#define PIN_NUM_BCKL 5, already set high using a wire.

//#define LCD_BK_LIGHT_ON_LEVEL   0

//To speed up transfers, every SPI transfer sends a bunch of lines. This define specifies how many. More means more memory use,
//but less overhead for setting up / finishing transfers. Make sure 240 is dividable by this.
#define PARALLEL_LINES 16

// Number of horizontal display lines sent in each SPI transfer.
// Larger values send more pixels per transfer, which reduces SPI overhead
// and can improve drawing speed, but requires a larger memory buffer.
// Keep this as a factor of 240 so the screen height divides evenly.
//240/16 = 15 lines each SPI transfer.


#define Y_MAX 320
#define X_MAX 240

//the lcd needs a bunch of command/argument values to be initialized. they stored in this struct/.

typedef struct {
    uint8_t cmd;        // The command byte sent to the LCD controller (e.g., 0x11 for Sleep Out)
    uint8_t data[16];   // Array of argument/parameter bytes that accompany the command (up to 16 bytes)
    uint8_t databytes;  // Multi-purpose control byte:
                        // - Lower bits (0-6): Number of valid bytes being used in the 'data' array
                        // - Bit 7 (MSB): If set, tells the driver to pause/delay after sending this command
                        // - 0xFF: Sentinel value used to signal the absolute end of the command list
} lcd_init_cmd_t;
// cmd is the byte that tells the lcd what to do
// for example: 0x29 means turn the screen on, or 0x11 means exit sleep mode.

// data[16] is a 16-byte array where each index is 1 byte large.
// It acts like a workspace to hold extra settings/parameters for the command.
// Reads the command, then checks how many bytes are needed.
// Grabs the settings from the active bytes in data[16] and sends them right after cmd.
// Checks if Bit 7 of the 'databytes' variable is flipped (HIGH); if it is, 
// the code pauses for a brief delay.
// A 'databytes' value of 0xFF is the signal to end the entire command list.

//databytes 
//the first 6 bits tells you the length or number of data bytes/arguments that need to be sent
//which is located in data[16], for example: how many of those 16 slots do i need to actually send?
//if databytes = 0x00, then I don't need to send any extra settings after command.
//if databytes = 0xFF, or 1111_1111, then we are at the end of the commmand list so stop updating the screen.
//if databytes = 0x03, it tells the program to read the first 3 items of the data array
//data[0], data[1], data[2] and send them right after the command. 

//if the 7th bit in databytes is set, then the code pauses for a brief moment then sends the command.
//this is so that the display/screen has a break to catch up with the instructions
//if you MCU sends the instructions too fast, the screen may not catch all the instructions.

typedef enum {
    LCD_TYPE_ILI = 1,  // Represents ILI display controllers (e.g., ILI9341), explicitly starts at 1
    LCD_TYPE_ST,  //not needed.     
    LCD_TYPE_MAX = 3,      // Boundary/max count marker, automatically assigned 3 for error-checking
} type_lcd_t;

//TFT Controller: ILI9341
//Touch Screen Controller: XPT2046


//place data into dynamic random access memory DRAM. 
//Constant data gets placed into DROm by default, which is not accessible by direct memory address (DMA)
//DMA in a computer stands for Direct Memory Access,
//a feature that allows hardware devices to transfer data directly to and 
// from the system's main memory (RAM) without needing the computer's Central Processing Unit (CPU) 
// to handle every single piece of data. 




DRAM_ATTR static const lcd_init_cmd_t ili_init_cmds[] = {
    /* Power control B, power control = 0, DC_ENA = 1 */
    {0xCF, {0x00, 0x83, 0X30}, 3},
    /* Power on sequence control,
     * cp1 keeps 1 frame, 1st frame enable
     * vcl = 0, ddvdh=3, vgh=1, vgl=2
     * DDVDH_ENH=1
     */
    {0xED, {0x64, 0x03, 0X12, 0X81}, 4},
    /* Driver timing control A,
     * non-overlap=default +1
     * EQ=default - 1, CR=default
     * pre-charge=default - 1
     */
    {0xE8, {0x85, 0x01, 0x79}, 3},
    /* Power control A, Vcore=1.6V, DDVDH=5.6V */
    {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
    /* Pump ratio control, DDVDH=2xVCl */
    {0xF7, {0x20}, 1},
    /* Driver timing control, all=0 unit */
    {0xEA, {0x00, 0x00}, 2},
    /* Power control 1, GVDD=4.75V */
    {0xC0, {0x26}, 1},
    /* Power control 2, DDVDH=VCl*2, VGH=VCl*7, VGL=-VCl*3 */
    {0xC1, {0x11}, 1},
    /* VCOM control 1, VCOMH=4.025V, VCOML=-0.950V */
    {0xC5, {0x35, 0x3E}, 2},
    /* VCOM control 2, VCOMH=VMH-2, VCOML=VML-2 */
    {0xC7, {0xBE}, 1},
    /* Memory access control, MX=0, MY=1, MV=0, ML=0, BGR=1, MH=0 (Portrait 240x320) */
    {0x36, {0x48}, 1},
    /* Pixel format, 16bits/pixel for RGB/MCU interface */
    {0x3A, {0x55}, 1},
    /* Frame rate control, f=fosc, 70Hz fps */
    {0xB1, {0x00, 0x1B}, 2},
    /* Enable 3G, disabled */
    {0xF2, {0x08}, 1},
    /* Gamma set, curve 1 */
    {0x26, {0x01}, 1},
    /* Positive gamma correction */
    {0xE0, {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0X87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00}, 15},
    /* Negative gamma correction */
    {0XE1, {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F}, 15},
    /* Column address set, SC=0, EC=0xEF */
    {0x2A, {0x00, 0x00, 0x00, 0xEF}, 4},
    /* Page address set, SP=0, EP=0x013F */
    {0x2B, {0x00, 0x00, 0x01, 0x3f}, 4},
    /* Memory write */
    {0x2C, {0}, 0},
    /* Entry mode set, Low vol detect disabled, normal display */
    {0xB7, {0x07}, 1},
    /* Display function control */
    {0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},
    /* Sleep out */
    {0x11, {0}, 0x80},
    /* Display on */
    {0x29, {0}, 0x80},
    {0, {0}, 0xff},
};

/* Send a command to the LCD. Uses spi_device_polling_transmit, which waits
 * until the transfer is complete.
 *
 * Since command transactions are usually small, they are handled in polling
 * mode for higher speed. The overhead of interrupt transactions is more than
 * just waiting for the transaction to complete.
 */

 //this function sends a command to the LCD, where we use the function 
 //spi_device_polling_transmit to send the command

 //spi_device_handle_t spi - tells the code which spi peripheral to use to talk to the screen.
 //cmd - 1 byte command to send to the lcd.
 //keep_cs_active, a flag if true keeps chip select pulled low after sending, meaning
 //"stay tuned" more data will be on the way after this command. 
 void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd, bool keep_cs_active) {

    esp_err_t ret; //variable to store the success/error returned by the SPI functions.
    spi_transaction_t t; //a struct required by the ESP_IDF SPI program.
    memset(&t, 0, sizeof(t)); //wipes the memory of the struct to zero, so 
    //old garbage data doesn't cause bugs.



    t.length = 8; //set the data length to 1 byte because we only send 1 byte at a time. 
    t.tx_buffer = &cmd; //ptr to the address of the command byte.
    t.user = (void*)0; //dk
    if (keep_cs_active) { //tells the program to keep the CS active, don't let go of the screen yet.
        t.flags = SPI_TRANS_CS_KEEP_ACTIVE; //keep chip select active after data transfer.
    }
    ret = spi_device_polling_transmit(spi, &t); //transmit the data in polling mode
    //which means the CPU waits until the data transfer finishes rather than using background interrupts.

    //then saves the result of the data transfer into ret.
    assert(ret == ESP_OK); //see if we have no issues.
    //if successful then we get back ESP_OK.
    //if fail, then assert() will crash so you know what goes wrong.
 }

//spi_transaction_t struct.
 /*
 typedef struct {
    uint32_t flags;                 // Bitwise flags to configure transaction options (e.g. SPI_TRANS_USE_RXDATA)
    uint16_t cmd;                   // Command data to send (length defined in spi_device_interface_config_t)
    uint64_t addr;                  // Address data to send (length defined in spi_device_interface_config_t)
    size_t length;                  // Total data length to transmit/receive, in BITS
    size_t rxlength;                // Total data length to receive, in BITS (defaults to 'length' if 0)
    void *user;                     // User-defined token/pointer, useful for tracking callbacks
    union {
        const void *tx_buffer;      // Pointer to transmit buffer
        uint8_t tx_data[4];         // Array containing data if SPI_TRANS_USE_TXDATA flag is set
    };
    union {
        void *rx_buffer;            // Pointer to receive buffer
        uint8_t rx_data[4];         // Array containing data if SPI_TRANS_USE_RXDATA flag is set
    };
} spi_transaction_t;
 */


 /* Send data to the LCD. Uses spi_device_polling_transmit, which waits until the
 * transfer is complete.
 *
 * Since data transactions are usually small, they are handled in polling
 * mode for higher speed. The overhead of interrupt transactions is more than
 * just waiting for the transaction to complete.
 */
void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len) {
    esp_err_t ret;
    spi_transaction_t t;
    if (len == 0) { //length of data is 0 then we ain't send nun.
        return;    //no need to send anything
    }
    memset(&t, 0, sizeof(t));       //zero out the struct so garbage values don't mess up things.
    t.length = len * 8;             //Len is in bytes, transaction length is in bits.
    t.tx_buffer = data;             //Data
    t.user = (void*)1;              //D/C needs to be set to 1
    ret = spi_device_polling_transmit(spi, &t); //Transmit the data and get the result back.
    assert(ret == ESP_OK);          //Should have had no issues.
}


//This function is called (in irq context!) just before a transmission starts. It will
//set the D/C line to the value indicated in the user field.
void lcd_spi_pre_transfer_callback(spi_transaction_t *t) {
    int dc = (int)t->user;
    gpio_set_level(PIN_NUM_DC, dc);
}


//this function asks the lcd for its ID, to actually prove its an ILI9341
//the screen will respond with a unique identification code, then this function 
//reads it back.
uint32_t lcd_get_id(spi_device_handle_t spi) {
    // When using SPI_TRANS_CS_KEEP_ACTIVE, bus must be locked/acquired
    spi_device_acquire_bus(spi, portMAX_DELAY); //lock the SPI path so that 
    //other tasks don't interrupt us.

    //sending the read command through SPI.
    lcd_cmd(spi, 0x04, true); //0x04 is the command to read.
    //the true part is for the CS to keep the active so it can read a response.

    spi_transaction_t t;
    memset(&t, 0, sizeof(t)); //clear garbage values.
    t.length = 8 * 3; //3 bytes worth of data.
    t.flags = SPI_TRANS_USE_RXDATA; //dk
    t.user = (void*)1; //user flag.

    esp_err_t ret = spi_device_polling_transmit(spi, &t); //executes read operation 
    // and checks for success.
    assert(ret == ESP_OK);

    // Release bus
    spi_device_release_bus(spi);

    //grabs the raw bytes inside the struct and casts them into a 32 bit integer, and returns the 
    //final ID number back to us. 
    return *(uint32_t*)t.rx_data;
}

//initialize the display 
void init_lcd(spi_device_handle_t spi) {
    int cmd = 0; //initialize our cmd to nothing.

    const lcd_init_cmd_t* lcd_init_cmds; //pointer to the struct lcd_init_cmd_t

    //initialize non-spio gpios
    gpio_config_t io_conf = {};
    //set them high at first?
    io_conf.pin_bit_mask = ((1ULL << PIN_NUM_DC) | (1ULL << PIN_NUM_RST));
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);


    //reset the display
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(100/ portTICK_PERIOD_MS);
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(100/ portTICK_PERIOD_MS);

       // Set command list for ILI9341
    printf("LCD ILI9341 initialization.\n");
    lcd_init_cmds = ili_init_cmds;

        //Send all the commands
    while (lcd_init_cmds[cmd].databytes != 0xFF) {
        lcd_cmd(spi, lcd_init_cmds[cmd].cmd, false);
        lcd_data(spi, lcd_init_cmds[cmd].data, lcd_init_cmds[cmd].databytes & 0x1F);
        if (lcd_init_cmds[cmd].databytes & 0x80) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        cmd++;
    }
}

/* To send a set of lines we have to send a command, 2 data bytes, another command, 2 more data bytes and another command
 * before sending the line data itself; a total of 6 transactions. (We can't put all of this in just one transaction
 * because the D/C line needs to be toggled in the middle.)
 * This routine queues these commands up as interrupt transactions so they get
 * sent faster (compared to calling spi_device_transmit several times), and at
 * the mean while the lines for next transactions can get calculated.
 */

 static void send_lines(spi_device_handle_t spi, int ypos, uint16_t *linedata) {
    esp_err_t ret;
    int x;
    //Transaction descriptors. Declared static so they're not allocated on the stack; we need this memory even when this
    //function is finished because the SPI driver needs access to it even while we're already calculating the next line.
    static spi_transaction_t trans[6];

    //In theory, it's better to initialize trans and data only once and hang on to the initialized
    //variables. We allocate them on the stack, so we need to re-init them each call.
    for (x = 0; x < 6; x++) {
        memset(&trans[x], 0, sizeof(spi_transaction_t));
        if ((x & 1) == 0) {
            //Even transfers are commands
            trans[x].length = 8;
            trans[x].user = (void*)0;
        } else {
            //Odd transfers are data
            trans[x].length = 8 * 4;
            trans[x].user = (void*)1;
        }
        trans[x].flags = SPI_TRANS_USE_TXDATA;
    }
    trans[0].tx_data[0] = 0x2A;         //Column Address Set
    trans[1].tx_data[0] = 0;            //Start Col High
    trans[1].tx_data[1] = 0;            //Start Col Low
    trans[1].tx_data[2] = (X_MAX - 1) >> 8;   //End Col High
    trans[1].tx_data[3] = (X_MAX - 1) & 0xff; //End Col Low
    trans[2].tx_data[0] = 0x2B;         //Page address set
    trans[3].tx_data[0] = ypos >> 8;    //Start page high
    trans[3].tx_data[1] = ypos & 0xff;  //start page low
    trans[3].tx_data[2] = (ypos + PARALLEL_LINES - 1) >> 8; //end page high
    trans[3].tx_data[3] = (ypos + PARALLEL_LINES - 1) & 0xff; //end page low
    trans[4].tx_data[0] = 0x2C;         //memory write
    trans[5].tx_buffer = linedata;      //finally send the line data
    trans[5].length = X_MAX * 2 * 8 * PARALLEL_LINES;  //Data length, in bits
#if CONFIG_LCD_BUFFER_IN_PSRAM
    trans[5].flags = SPI_TRANS_DMA_USE_PSRAM; //using PSRAM
#else
    trans[5].flags = 0; //undo SPI_TRANS_USE_TXDATA flag
#endif

    //Queue all transactions.
    for (x = 0; x < 6; x++) {
        ret = spi_device_queue_trans(spi, &trans[x], portMAX_DELAY);
        assert(ret == ESP_OK);
    }

    //When we are here, the SPI driver is busy (in the background) getting the transactions sent. That happens
    //mostly using DMA, so the CPU doesn't have much to do here. We're not going to wait for the transaction to
    //finish because we may as well spend the time calculating the next line. When that is done, we can call
    //send_line_finish, which will wait for the transfers to be done and check their status.
}

static void send_line_finish(spi_device_handle_t spi)
{
    spi_transaction_t *rtrans;
    esp_err_t ret;
    //Wait for all 6 transactions to be done and get back the results.
    for (int x = 0; x < 6; x++) {
        ret = spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
        assert(ret == ESP_OK);
        //We could inspect rtrans now if we received any info back. The LCD is treated as write-only, though.
    }
}



//we allocate two ping pong empty buffers for the ram so that the dma hardware and cpu
//swap each time dma reads the bytes 

//so the cpu decodes the image into bytes and places it into buffer 1
//buffer 2 is empty
//dma hardware reads buffer 1 and places those bytes onto spi bus and pushes it to the LCD
//while dma hardware is reading, the cpu decodes the next image into 
//bytes into the empty buffer 1

//then the cycle repeats, this happens in parallel so its very fast, 
// and solves the problem of the cpu decoding the image and allocating memory then the 
//dma hardware reading bytes then sending onto spi bus.

//2 pointers for each of our buffers
static uint16_t *s_dma_lines[2] = {NULL, NULL};

//the very first time this function runs, it allocates memory for both buffers.
// Routine to draw the clean decoded image to the LCD once.
static void display_pretty_colors(spi_device_handle_t spi) {
    //if our buffers are empty.
    if (s_dma_lines[0] == NULL || s_dma_lines[1] == NULL) {
#if CONFIG_LCD_BUFFER_IN_PSRAM //if this is set, then we allocate memory in psram
        uint32_t mem_cap = MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA;
#else //else we allocate memory using internal sram.
        uint32_t mem_cap = MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA;
#endif
//allocate memory for our 2 buffers
//You allocate 7.68 KB because that is the exact memory size 
// needed to hold 16 lines of 240-pixel RGB565 color data, 
// allowing you to render the 320-line screen in 20 fast, memory-efficient slices.
//the old image bytes are just overwritten by the next image.

//to draw 1 frame, the screen is sliced into 20 chunks, 320 lines / 16 lines = 20 chunks
//the height is 320 pixels or has 320 lines rights
//instead of allocating memory for 320 lines at once (which takes too much memory)
//the screen is sliced into 16 line chynks

//we defined 2 buffers each 7.68kb each holding 16 lines worth of image bytes.

        for (int i = 0; i < 2; i++) {
            if (s_dma_lines[i] == NULL) { //checks if the buffer is empty before allocating.
                s_dma_lines[i] = spi_bus_dma_memory_alloc(LCD_HOST, X_MAX * PARALLEL_LINES * sizeof(uint16_t), mem_cap);
                assert(s_dma_lines[i] != NULL);
            }
            //x_max = 240 *parallel lines is 16 * sizeof 16 bit int is 2 bytes
            // so 7.68kb alloacted per buffer per slice, there about 20 slices for entire image.
            //this specific amount each buffer because one full image byte is about 153
        }
    }

    //this tracks which buffer the dma hardware is currently transmitting, it starts at 
    //-1 because we haven't sent anything yet with the dma.
    int sending_line = -1;

    //this tracks which buffer the cpu is currently filling, it starts at buffer 0 
    int calc_line = 0;

    // Render the entire image once across all 320 vertical lines
    for (int y = 0; y < Y_MAX; y += PARALLEL_LINES) {
        pretty_effect_calc_lines(s_dma_lines[calc_line], y, 0, PARALLEL_LINES);
        if (sending_line != -1) { //if we are actually sending lines, then we send via spi.
            send_line_finish(spi);
        }
        
        sending_line = calc_line;
        calc_line = (calc_line == 1) ? 0 : 1;
        send_lines(spi, y, s_dma_lines[sending_line]);
    }
    if (sending_line != -1) {
        send_line_finish(spi);
    }
}

static spi_device_handle_t spi;

extern bool auto_running;

static void animation_task(void *pvParameters) {
    int current_frame = 0;
    page_t last_rendered_page = (page_t)-1;

    while(1) {
        if (current_page == PAGE_MENU) {
            // Main menu: animate 15-frame rover and pulse the hover pill cursor
            decode_image(current_frame, &pixels);
            display_pretty_colors(spi);

            current_frame++;
            if (current_frame >= TOTAL_FRAMES) {
                current_frame = 0;
            }
            last_rendered_page = PAGE_MENU;
            vTaskDelay(pdMS_TO_TICKS(40));
        } 
        else if (current_page == PAGE_MANUAL || current_page == PAGE_MANUAL_DATA || current_page == PAGE_AUTO ||
                 current_page == PAGE_AUTO_DATA || current_page == PAGE_IMU_DATA) {
            // Decode the background image once upon entering the page
            if (last_rendered_page != current_page) {
                decode_image(0, &pixels);
                last_rendered_page = current_page;
            }
            // Continuously redraw live telemetry numbers, RF stats & overlays in real time!
            display_pretty_colors(spi);
            vTaskDelay(pdMS_TO_TICKS(35));
        }
        else {
            // Showcase QR static pages (LinkedIn, GitHub, Empty left, etc.)
            // Decode and redraw only when entering a new page
            if (current_page != last_rendered_page) {
                decode_image(0, &pixels);
                display_pretty_colors(spi);
                last_rendered_page = current_page;
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}


void init_lcd_driver(void) {
    esp_err_t ret;
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = PARALLEL_LINES * X_MAX * 2 + 8
    };
    spi_device_interface_config_t devcfg = {
#ifdef CONFIG_LCD_OVERCLOCK
        .clock_speed_hz = 26 * 1000 * 1000,     //Clock out at 26 MHz
#else
        .clock_speed_hz = 26 * 1000 * 1000,     //Clock out at 26 MHz
#endif

//initialized the spi here
        .mode = 0,                              //SPI mode 0
        .spics_io_num = PIN_NUM_CS,             //CS pin
        .queue_size = 7,                        //We want to be able to queue 7 transactions at a time
        .pre_cb = lcd_spi_pre_transfer_callback, //Specify pre-transfer callback to handle D/C line
    };

    //Initialize the SPI bus
    //we initialize the spi bus with the hardware direct memory address. 
    // SPI_DMA_CH_AUTO tells ESP-IDF to automatically select and assign an available
    // hardware DMA channel to this SPI bus so transfers happen in the 
    // background without CPU intervention.
    ret = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret); //check if this is successful or not.

    //Attach the LCD to the SPI bus
    ret = spi_bus_add_device(LCD_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);

    //Initialize the LCD
    init_lcd(spi);

    // Initialize & decode image
    pretty_effect_init();
    
    // Draw the static image once to the screen
    display_pretty_colors(spi);

    // Launch the animation task in the background on core 1
    xTaskCreatePinnedToCore(animation_task, "anim_task", 4096, NULL, 2, NULL, 1);
    //using core 1 because core 0 is the esp-now/wifi
    //4096 is the number of bytes allocated for this task.
}

/*
BaseType_t xTaskCreatePinnedToCore(
    TaskFunction_t pvTaskCode,        // 1. Task function pointer, the function to run.
    const char * const pcName,        // 2. Descriptive text name
    const uint32_t usStackDepth,      // 3. Stack size (in bytes) allocated for this task
    void * const pvParameters,        // 4. Arguments passed to task, pointers you want to pass into the function.

    NULL means no parameters


    UBaseType_t uxPriority,           // 5. Priority level, higher level is a higher priority, 2 is a gentle priority.
    TaskHandle_t * const pvCreatedTask,// 6. Task handle pointer (output) //how you want to control the task or end the task later.
    const BaseType_t xCoreID          // 7. Core ID to pin to (0 or 1) which core to use.

    0 - Core 0 (runs wifi and the esp-now)
    1 - Core 1 (APP CPU)
);

BaseType_t xTaskCreate(
    TaskFunction_t pvTaskCode,         // 1. Function to run
    const char * const pcName,         // 2. Descriptive text name
    const uint32_t usStackDepth,       // 3. Stack size (in bytes) allocated for the task.
    void * const pvParameters,         // 4. Arguments passed to task (or NULL)
    UBaseType_t uxPriority,            // 5. Priority level (e.g. 1 to 5)
    TaskHandle_t * const pvCreatedTask // 6. Task handle pointer (or NULL)
);

when to use whic, 

xTaskCreate for quick, lightweight tasks
xTaskCreatePinnedToCore for heavy or timing sensitive tasks like animation_task or motor control, where you 
can guarantee it doesn't fight wifi/radio on core 0


*/

