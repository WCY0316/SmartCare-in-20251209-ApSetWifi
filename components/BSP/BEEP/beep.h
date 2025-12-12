#ifndef BEEP_H__
#define BEEP_H__

// #define BUZZER_POWER_PIN 47

#define TEST_DEBUG 0      // 调试
#define HEBEI_TYPE_TEST 0 // 河北型检 24.5.25

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

#define BUZZER_FREQ_PIN 47 //
#define BUZZER_TIMER LEDC_TIMER_0
#define BUZZER_MODE LEDC_LOW_SPEED_MODE
#define BUZZER_CHANNEL LEDC_CHANNEL_1
#define BUZZER_DUTY_RES LEDC_TIMER_13_BIT // Set duty resolution to 13 bits

#define BUZZER_FREQ_CLICK 1000
#define BUZZER_FREQ_SUCC 2000
#define BUZZER_FREQ_ERROR 200

#define BUZZER_TIME 20 // 蜂鸣器持续时间200ms

#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <esp_system.h>
#include "driver/rmt.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"

typedef enum
{
    BST_CLICK,
    BST_SUCC,
    BST_ERROR,
    BST_MAX
} BUZZER_SOUND_TYPE;

extern volatile uint8_t BuzzerEnable;
extern volatile uint8_t BuzzerCnt;

void InitBuzzer(void);
void StartBuzzer(BUZZER_SOUND_TYPE type);
void StopBuzzer(void);

#endif
