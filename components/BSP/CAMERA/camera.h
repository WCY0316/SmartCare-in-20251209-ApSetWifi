#ifndef _CAMERA_H__
#define _CAMERA_H__
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_task_wdt.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "freertos/event_groups.h"

// 摄像头管脚定义
#define CAM_PIN_PWDN -1  // power down is not used
#define CAM_PIN_RESET -1 // software reset will be performed

// #define CAM_PIN_PWDN 36
// #define CAM_PIN_RESET 35
#define CAM_PIN_XCLK 15
#define CAM_PIN_SIOD 4
#define CAM_PIN_SIOC 5

#define CAM_PIN_D7 16
#define CAM_PIN_D6 17
#define CAM_PIN_D5 18
#define CAM_PIN_D4 12
#define CAM_PIN_D3 10
#define CAM_PIN_D2 8
#define CAM_PIN_D1 9
#define CAM_PIN_D0 11
#define CAM_PIN_VSYNC 6
#define CAM_PIN_HREF 7
#define CAM_PIN_PCLK 13

void SavePicToFlash(uint8_t *PicFrame, uint32_t Size);
uint32_t IsPicCached(void);
void ReadPicFromFlash(uint8_t *PicFrame, uint32_t Size);
int CheckJpgCaching(void);
void VCameraTask(void *pvParam);
esp_err_t InitCamera(void);
uint8_t TakePic(void);

extern QueueHandle_t camera_evt_queue;

extern uint32_t flashPictureSize;

#endif
