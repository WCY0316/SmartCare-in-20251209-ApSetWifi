#ifndef BSP_LED_H
#define BSP_LED_H

#include <driver/gpio.h>
#include <driver/i2c.h>
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include "string.h"

// Define your AW9523 GPIO pins
#define AW9523_SDA_PIN 42
#define AW9523_SCL_PIN 41
#define AW9523_RST_PIN 40

// AW9523 I2C address
#define AW9523_I2C_ADDR 0x5B
// AW9523B I2C address
#define AW9523B_I2C_ADDR AW9523_I2C_ADDR

// AW9523B registers
#define AW9523_MODE_SWITCH_P0_REG 0x12
#define AW9523_MODE_SWITCH_P1_REG 0x13
#define AW9523_DIM0_REG 0x20
#define AW9523_DIM1_REG 0x21
#define AW9523_DIM2_REG 0x22
#define AW9523_DIM3_REG 0x23
#define AW9523_DIM4_REG 0x24
#define AW9523_DIM5_REG 0x25
#define AW9523_DIM6_REG 0x26
#define AW9523_DIM7_REG 0x27
#define AW9523_DIM8_REG 0x28
#define AW9523_DIM9_REG 0x29
#define AW9523_DIM10_REG 0x2A
#define AW9523_DIM11_REG 0x2B
#define AW9523_DIM12_REG 0x2C
#define AW9523_DIM13_REG 0x2D
#define AW9523_DIM14_REG 0x2E
#define AW9523_DIM15_REG 0x2F

// AW9523 registers
#define AW9523_MODE1_REG 0x00
#define AW9523_PWM0_REG 0x02
#define AW9523_PWM1_REG 0x03
#define AW9523_PWM2_REG 0x04

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

#define RGB_BLINK_TIME 10 // led闪烁时间

typedef enum
{
    LED_OFF,
    LED_ON
} LED_STATE;

typedef enum
{
    LED_NOT_BLINK,
    LED_BLINK,
} LED_BLINK_OR_NOT;
typedef enum
{
    rgbLed_Heating,  // 加热灯
    rgbLed_Reacting, // 反应灯
    rgbLed_Wifi,
    rgbLed_Index_Max
} RGB_LED_INDEX;
// 灯颜色
typedef enum
{
    LED_COLOR_RED,
    LED_COLOR_GREEN,
    LED_COLOR_BLUE,
    LED_COLOR_OFF,
    LED_COLOR_MAX
} LED_COLOR;

typedef struct
{
    uint8_t NeedAction;
    uint8_t BlinkFlag;
    uint8_t BlinkCnt;
    uint8_t R;
    uint8_t G;
    uint8_t B;
} COLOR_LED_CTRL_STRUCT;

extern volatile int blinkCnt, flipFlag;
extern COLOR_LED_CTRL_STRUCT *pConfig;
extern COLOR_LED_CTRL_STRUCT ColorLedConfig[rgbLed_Index_Max];

esp_err_t aw9523_init(void);
// esp_err_t aw9523_set_rgb(uint8_t led_num, uint8_t red, uint8_t green, uint8_t blue);
esp_err_t aw9523_set_rgb(uint8_t ledIndex, uint8_t R, uint8_t G, uint8_t B);
esp_err_t aw9523b_set_led_mode();
esp_err_t aw9523b_set_dim(uint8_t pin, uint8_t value);
esp_err_t aw9523_set_pwm(uint8_t pin, uint8_t value);
esp_err_t aw9523b_set_led_mode_for_pin(uint8_t pin);

esp_err_t SetRGBLedColor(RGB_LED_INDEX index, LED_COLOR color, uint8_t IsBlink);
void InitLed(void);

void CameraLedCtrl(LED_STATE ledState);
void BreathLedCtrl(LED_STATE ledState);
void SelfCheck(void);

#endif
