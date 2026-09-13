/*
 * SPI Master example: JPEG decoder using TJpgDec
 */

#include "decode_image.h"
#include "tjpgd.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include "transmit_data.h"

extern bool auto_running;


// Binary included JPEG file in flash
//just change frame1 to whatever the img file is called to change the image.

//change in this file, platformio.ini and CMakeLists.TXT
// Note: hyphens '-' and dots '.' in filenames are automatically converted to underscores '_' in assembly symbols

    extern const uint8_t f1_start[] 
    asm("_binary_frame1_jpg_start");
    extern const uint8_t f1_end[]   
    asm("_binary_frame1_jpg_end");

    extern const uint8_t f2_start[]
    asm("_binary_frame2_jpg_start");
    extern const uint8_t f2_end[]   
    asm("_binary_frame2_jpg_end");

    extern const uint8_t f3_start[] 
    asm("_binary_frame3_jpg_start");
    extern const uint8_t f3_end[]   
    asm("_binary_frame3_jpg_end");

    extern const uint8_t f4_start[]
    asm("_binary_frame4_jpg_start");
    extern const uint8_t f4_end[]   
    asm("_binary_frame4_jpg_end");

    extern const uint8_t f5_start[] 
    asm("_binary_frame5_jpg_start");
    extern const uint8_t f5_end[]   
    asm("_binary_frame5_jpg_end");

    extern const uint8_t f6_start[]
    asm("_binary_frame6_jpg_start");
    extern const uint8_t f6_end[]   
    asm("_binary_frame6_jpg_end");

    extern const uint8_t f7_start[] 
    asm("_binary_frame7_jpg_start");
    extern const uint8_t f7_end[]   
    asm("_binary_frame7_jpg_end");

    extern const uint8_t f8_start[]
    asm("_binary_frame8_jpg_start");
    extern const uint8_t f8_end[]   
    asm("_binary_frame8_jpg_end");

    extern const uint8_t f9_start[]
    asm("_binary_frame9_jpg_start");
    extern const uint8_t f9_end[]   
    asm("_binary_frame9_jpg_end");

    extern const uint8_t f10_start[]
    asm("_binary_frame10_jpg_start");
    extern const uint8_t f10_end[]   
    asm("_binary_frame10_jpg_end");

    extern const uint8_t f11_start[]
    asm("_binary_frame11_jpg_start");
    extern const uint8_t f11_end[]   
    asm("_binary_frame11_jpg_end");

    extern const uint8_t f12_start[]
    asm("_binary_frame12_jpg_start");
    extern const uint8_t f12_end[]   
    asm("_binary_frame12_jpg_end");

    extern const uint8_t f13_start[]
    asm("_binary_frame13_jpg_start");
    extern const uint8_t f13_end[]   
    asm("_binary_frame13_jpg_end");

    extern const uint8_t f14_start[]
    asm("_binary_frame14_jpg_start");
    extern const uint8_t f14_end[]   
    asm("_binary_frame14_jpg_end");

    extern const uint8_t f15_start[]
    asm("_binary_frame15_jpg_start");
    extern const uint8_t f15_end[]   
    asm("_binary_frame15_jpg_end");

    extern const uint8_t left_start[]
    asm("_binary_left_jpg_start");
    extern const uint8_t left_end[]
    asm("_binary_left_jpg_end");

    extern const uint8_t leftright_start[]
    asm("_binary_leftright_jpg_start");
    extern const uint8_t leftright_end[]
    asm("_binary_leftright_jpg_end");

    extern const uint8_t linkedin_start[]
    asm("_binary_linkedin_jpg_start");
    extern const uint8_t linkedin_end[]
    asm("_binary_linkedin_jpg_end");

    extern const uint8_t github_start[]
    asm("_binary_github_jpg_start");
    extern const uint8_t github_end[]
    asm("_binary_github_jpg_end");

    extern const uint8_t blankfirstpage_start[]
    asm("_binary_blankfirstpage_jpg_start");
    extern const uint8_t blankfirstpage_end[]
    asm("_binary_blankfirstpage_jpg_end");

    extern const uint8_t secondpagedata_start[]
    asm("_binary_secondpagedata_jpg_start");
    extern const uint8_t secondpagedata_end[]
    asm("_binary_secondpagedata_jpg_end");

    extern const uint8_t autofirst_start[]
    asm("_binary_autofirst_jpg_start");
    extern const uint8_t autofirst_end[]
    asm("_binary_autofirst_jpg_end");

    extern const uint8_t autofirststop_start[]
    asm("_binary_autofirststop_jpg_start");
    extern const uint8_t autofirststop_end[]
    asm("_binary_autofirststop_jpg_end");



//create array with ptrs to the images
static const uint8_t *frame_starts[TOTAL_FRAMES] = {
    f1_start,
    f2_start,
    f3_start,
    f4_start,
    f5_start,
    f6_start,
    f7_start,
    f8_start,
    f9_start,
    f10_start,
    f11_start,
    f12_start,
    f13_start,
    f14_start,
    f15_start
};

// the endings
static const uint8_t *frame_ends[TOTAL_FRAMES] = {
    f1_end,
    f2_end,
    f3_end,
    f4_end,
    f5_end,
    f6_end,
    f7_end,
    f8_end,
    f9_end,
    f10_end,
    f11_end,
    f12_end,
    f13_end,
    f14_end,
    f15_end,
};

static const char *TAG = "ImageDec";
// Struct to pass to decoder callback
typedef struct {
    const uint8_t *inData;
    uint32_t inPos;
    uint32_t inLen;
    uint16_t **outData;
} JpegDev;

// Stream input function for TJpgDec
static size_t infunc(JDEC *decoder, uint8_t *buf, size_t len) {
    JpegDev *jd = (JpegDev *)decoder->device;
    if (jd->inPos + len > jd->inLen) {
        len = jd->inLen - jd->inPos;
    }
    if (buf != NULL && len > 0) {
        memcpy(buf, jd->inData + jd->inPos, len);
    }
    jd->inPos += len;
    return len;
}

// Block output function for TJpgDec
static int outfunc(JDEC *decoder, void *bitmap, JRECT *rect) {
    JpegDev *jd = (JpegDev *)decoder->device;
    uint16_t *in = (uint16_t *)bitmap;
    for (int y = rect->top; y <= rect->bottom; y++) {
        for (int x = rect->left; x <= rect->right; x++) {
            if (x < IMAGE_W && y < IMAGE_H) {
                // Byte swap for ILI9341 SPI
                uint16_t color = *in++;
                (*jd->outData)[y * IMAGE_W + x] = (color >> 8) | (color << 8);
            } 
            else {
                in++;
            }
        }
    }
    return 1; // Continue decoding
}

esp_err_t decode_image(int frame_idx, uint16_t **pixels) {
    char *work = NULL;
    JDEC decoder;
    JpegDev jd;
    //*pixels = NULL;
    esp_err_t ret = ESP_OK;

    // Workspace required by TJpgDec
    work = (char *)calloc(3800, sizeof(char));
    if (work == NULL) {
        ESP_LOGE(TAG, "Cannot allocate workspace");
        ret = ESP_ERR_NO_MEM;
        goto err;
    }

    // Allocate memory for the 320x240 image

    //we allocating memory for the images, 320*240*bits per pixel (16) = 76800*16 bits / 8bits = 154kb  
    
    if (*pixels == NULL) { //if empty we call calloc to request 154Kb of ram from teh esp32
        //for the first frame, if its not null then the code skips the line below

        //and reuses the same 154kb memory it requested before
        *pixels = (uint16_t *)calloc(IMAGE_H * IMAGE_W, sizeof(uint16_t));
        if (*pixels == NULL) { //checks if calloc actually worked or did the esp32 run out of ram.

        ESP_LOGE(TAG, "Cannot allocate pixel memory");
        ret = ESP_ERR_NO_MEM;
        goto err;
        }
    }
    
    

    //jd.inData = frame_starts[frame_idx];
    //jd.inLen = frame_ends[frame_idx] - frame_starts[frame_idx];
    // Select image source based on current_page:
    switch (current_page) {
        case PAGE_MENU:
            // Menu page: play the 15-frame animation
            jd.inData = frame_starts[frame_idx];
            jd.inLen  = frame_ends[frame_idx] - frame_starts[frame_idx];
            break;


        // 1st page of any of the modes (double arrows)
        case PAGE_MANUAL:
            //specific dashboard for this page
            jd.inData = blankfirstpage_start;
            jd.inLen = blankfirstpage_end - blankfirstpage_start;
            break;
        case PAGE_AUTO:
            jd.inData = autofirst_start;
            jd.inLen  = autofirst_end - autofirst_start;
            break;
        case PAGE_IMU:
            jd.inData = leftright_start; // the page with double arrows
            jd.inLen  = leftright_end - leftright_start;
            break;

        // 2nd page of any of the modes (single left arrow)
        case PAGE_MANUAL_DATA:
            jd.inData = secondpagedata_start;
            jd.inLen = secondpagedata_end - secondpagedata_start;
            break;

            
        case PAGE_AUTO_DATA:
        case PAGE_IMU_DATA:
        case PAGE_LEFTPAGE:
            jd.inData = left_start;
            jd.inLen  = left_end - left_start;
            break;

        case PAGE_GITHUB:
            // Page 1: GitHub Repo page
            jd.inData = github_start;
            jd.inLen  = github_end - github_start;
            break;

        case PAGE_LINKEDIN:
            // Page 2: LinkedIn QR page
            jd.inData = linkedin_start;
            jd.inLen  = linkedin_end - linkedin_start;
            break;

        default:
            jd.inData = left_start;
            jd.inLen  = left_end - left_start;
            break;
    }
    jd.inPos = 0;
    jd.outData = pixels;

    //zero out the decoder so that decoder.swap is not random stack garbage
    memset(&decoder, 0, sizeof(JDEC));
    decoder.swap=0;


    // Prepare the JPEG decoder
    JRESULT res = jd_prepare(&decoder, infunc, work, 3800, &jd);
    if (res != JDR_OK) {
        ESP_LOGE(TAG, "jd_prepare failed (%d)", res);
        ret = ESP_FAIL;
        goto err;
    }

    // Decompress the JPEG image
    res = jd_decomp(&decoder, outfunc, 0);
    if (res != JDR_OK) {
        ESP_LOGE(TAG, "jd_decomp failed (%d)", res);
        ret = ESP_FAIL;
        goto err;
    }

    free(work);
    return ESP_OK;

err:
    if (work) free(work);
    if (*pixels) free(*pixels);
    *pixels = NULL;
    return ret;
}
