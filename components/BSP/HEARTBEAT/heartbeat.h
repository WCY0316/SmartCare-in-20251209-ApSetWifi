#ifndef __HEARTBEAT_H__
#define __HEARTBEAT_H__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"

// 接口函数声明
BaseType_t InitHeartbeatTim(uint32_t interval_s);
void heartbeat_deinit(void);

#endif // __HEARTBEAT_H__