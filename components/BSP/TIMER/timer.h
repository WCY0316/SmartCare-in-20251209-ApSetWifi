#ifndef _TIMER_H__
#define _TIMER_H__

#include <esp_log.h>
#include <esp_system.h>
#include <sys/param.h>
#include <string.h>
#include <esp_timer.h>
#include "esp_task_wdt.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "bsp_gpio.h"

#define WIFI_LINK_MAX_SECOND CONVERT_TO_SECOND(10) // WIFI连接最大时间
#define TCP_LINK_MAX_SECOND CONVERT_TO_SECOND(20)  // TCP连接最大时间

extern volatile uint8_t react_time_min;

// #define REACT_TIME CONVERT_TO_SECOND(10 * 60)             // 检测时间
#define STOP_HEATING_COUNTDOWN CONVERT_TO_SECOND(30 * 60) // 待机时间

// 计时相关
volatile typedef struct
{
    uint32_t ReactingTime;    // 反应计时
    uint32_t ReactTimeFreeze; // 用于记录中途暂停时的时间
    uint32_t WiFiLinkTime;    // WIFI连接计时
    uint32_t TcpLinkTime;     // TCP连接计时
    uint32_t AutoHeatTime;    // 自动加热持续时间
} COUNT_DOWN_STRUCT;

// 结果相关
volatile typedef struct
{
    uint8_t StartReacting;
    uint8_t ReactFinishFlag;
    uint8_t WiFiLinkSate;
    uint8_t StopAutoHeating;
} USER_EVENT;

extern COUNT_DOWN_STRUCT Cd;
extern USER_EVENT UserEvent;

void InitTimer(void);
void StartReactCountDown(uint32_t reactMin);

#endif
