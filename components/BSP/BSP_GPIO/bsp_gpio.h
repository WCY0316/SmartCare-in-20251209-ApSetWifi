#ifndef _BSP_GPIO_H__
#define _BSP_GPIO_H__
#include <esp_log.h>
#include <esp_system.h>
#include <sys/param.h>
#include <string.h>
#include "driver/gpio.h"

#define ResetLongPressJudging() KeyState.PressCnt = KEY_KEEP_PRESSING_TIME
#define ResetMultiPressJudging() KeyState.ReleaseCnt = KeyState.NextPressCnt = EACH_PRESS_PERIOD

#define CONVERT_TO_SECOND(second) (100 * second)
#define KEY_KEEP_PRESSING_TIME CONVERT_TO_SECOND(5)
#define MULTI_PRESS_NUM_FOR_DEBUG 5            // 连击次数
#define MULTI_PRESS_NUM_FOR_Start 2            // 开始检测的连击次数
#define EACH_PRESS_PERIOD CONVERT_TO_SECOND(3) // 连击时，每相邻两次须在该时间内完成

// GPIO定义-输入
#define START_ENABLE_INT_PIN 2
#define DOOR_KEY_INT_PIN 39 // 开盖检测管脚

// GPIO定义-输出
#define BREATH_LED_PIN 14 // 呼吸灯,IO14
#define CAMERA_LED_PIN 38 // 拍照闪光灯

// 蜂鸣器
#define BUZZER_POWER_PIN 47

#define ESP_INTR_FLAG_DEFAULT 0

#define GPIO_INPUT_IO_0 DOOR_KEY_INT_PIN
#define GPIO_INPUT_IO_1 START_ENABLE_INT_PIN
#define GPIO_INPUT_PIN_SEL ((1ULL << GPIO_INPUT_IO_0) | (1ULL << GPIO_INPUT_IO_1))
#define GPIO_OUTPUT_PIN_SEL ((1ULL << CAMERA_LED_PIN) | (1ULL << BREATH_LED_PIN) | (1ULL << BUZZER_POWER_PIN)) // | (1ULL<<BUZZER_FREQ_PIN))

// wcy debug
volatile typedef struct
{
    uint32_t PressCnt;        // 长按计数
    uint32_t ReleaseCnt;      // 两次点击的间隔时间
    uint32_t NextPressCnt;    // 下一次点击须在该时间内到来
    uint32_t MultiPressNum;   // 连击次数
    uint8_t BtnSingleClicked; // 单击触发
    uint8_t BtnDoubleClicked; // 双击触发
    uint8_t BtnLongClicked;   // 长按触发
    uint8_t BtnMultiClicked;  // 连击触发
} KEY_PRESS_STATE;

extern KEY_PRESS_STATE KeyState;

void VGpioTask(void *arg);
void InitGpio(void);
void KeyPressStateMonitor(void);
#endif
