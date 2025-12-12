#ifndef _USB_COM_H__
#define _USB_COM_H__
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
// #include "esp_heap_task_info.h"
#include "esp_partition.h"
#include "esp_err.h"
#include "freertos/event_groups.h"

// extern EventGroupHandle_t xEventGroup;
extern volatile bool isSuccSetSN;

#define RX_BUFFER_SIZE 64

void usb_comm_init(void);
void usb_send_data(const char *data, size_t len);
void usb_receive_data(void);
void VUsbTask(void *arg);
#endif
