#ifndef DS18B20_H_
#define DS18B20_H_
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"

float ds18b20_get_temp(void);

#endif
