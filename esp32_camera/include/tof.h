#ifndef TOF_H
#define TOF_H
#include <stdint.h>
#include "esp_err.h"
esp_err_t tof_init(void);
uint16_t tof_get_distance(void);


#endif