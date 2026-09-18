#ifndef CAMERA_H
#define CAMERA_H
#include "esp_err.h"
#include "esp_camera.h"

esp_err_t init_camera(void);
camera_fb_t* camera_take_picture(void);
void camera_return_picture(camera_fb_t *fb);
#endif 