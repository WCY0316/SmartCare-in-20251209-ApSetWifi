#include <stdio.h>
#include <esp_log.h>
#include <math.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "heat.h"
#include "ds18b20.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "bsp_led.h"
#include "bsp_log.h"

// 加热控制
static const char *TAG = "HEAT";

// volatile float temperatureGoal = 46.0;
volatile float temperatureGoal = 20.0;

uint8_t AutoHeatEnable = 0; // 自动控温
// adc_oneshot_unit_handle_t adc1_handle;

// static int adc_raw[2][10];
// static int voltage[2][10];
// static int do_calibration = 0;
// static adc_cali_handle_t adc1_cali_handle = NULL;

static void InitHeatPwm(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = HEAT_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = HEAT_CTRL_PIN,
        .duty = 0, // Set duty to 0%
        .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void HeatingCtrl(uint8_t pwmRatio)
{
    uint32_t fullduty = (1 << LEDC_DUTY_RES) - 1;
    uint32_t duty;

    if (pwmRatio > 90)
        pwmRatio = 90;
    duty = fullduty * pwmRatio / 100;
    printf("heat ratio:%d, pwm=%ld\n", pwmRatio, duty);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

void InitHeat(void)
{
    uint8_t ret = 0;
    float temp = 0;
    uint64_t ser_id;

    InitHeatPwm();
    esp_rom_gpio_pad_select_gpio(DS18B20_PIN);

    // 采集温度值
    temp = ds18b20_get_temp();
    ESP_LOGI(TAG, "T=%f\n", temp); // 显示温度

    HeatingCtrl(0);

    // //温度AD配置
    // adc_oneshot_unit_init_cfg_t init_config1 = {
    //     .unit_id = ADC_UNIT_1,
    // };
    // ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    // adc_oneshot_chan_cfg_t temperatureConfig = {
    //     .bitwidth = ADC_BITWIDTH_DEFAULT,
    //     .atten = EXAMPLE_ADC_ATTEN,
    // };
    // ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, TEMPERATURE_ADC_CHAN, &temperatureConfig));

    // do_calibration = example_adc_calibration_init(ADC_UNIT_1, EXAMPLE_ADC_ATTEN, &adc1_cali_handle);
}

static uint8_t sHeatTempOk = 0;

int IsTemperatureOK(void)
{
    return (sHeatTempOk ? 1 : 0);
}
void SetHeatTempOK(uint8_t ok)
{
    sHeatTempOk = ok; // wcy debug2025.4.1
}
void VHeatTask(void *pvParam)
{
    uint32_t heatingFlag = 100;
    uint8_t stateChanged = 1;
    uint8_t ledColorRefreash = 100;
    float ds18T = 0, ds18TBak = 0, temp = 0;
    uint32_t cnt = 100, cnt2 = 100;
    uint32_t t = 0;

    printf("heat task running\n");
    printf("HEAT_task stack size is %ld \r\n", (int32_t)uxTaskGetStackHighWaterMark(NULL));
    printf("HEAT target temperature: %.2f\n", temperatureGoal);
    while (1)
    {

        ds18T = ds18b20_get_temp();
        // ds18T = 48.0;

        if ((ds18T > -10) && (ds18T < 60))
        {
            if (fabs(ds18T - ds18TBak) < 2)
                temp = ds18T; // 如果温度变化小于2度，使用当前温度
        }
        else
        {
            temp = ds18TBak; // 如果温度异常，使用上次温度
        }
        ds18TBak = ds18T;

        if (++cnt > 2)
        {
            cnt = 0;
            printf("T_%ld: %.1f\n", ++t, temp);
            if (++cnt2 > 20)
            {
                cnt2 = 0;
                write_log("INFO", TAG, "T_%ld: %.1f\n", t, temp);
            }
        }

        if (!AutoHeatEnable)
        {
            if (stateChanged)
            {
                stateChanged = 0;
                heatingFlag = 100;
                ledColorRefreash = 100;
                HeatingCtrl(0);
                SetRGBLedColor(rgbLed_Heating, LED_COLOR_OFF, LED_NOT_BLINK);
                printf("exit Autoheating\n");
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        stateChanged = 1;

        // ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, TEMPERATURE_ADC_CHAN, &adc_raw[0][0]));
        // ESP_LOGI(TAG, "AD Raw: %d", adc_raw[0][0]);+
        // if (do_calibration)
        // {
        //     ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw[0][0], &voltage[0][0]));
        //     // ESP_LOGI(TAG, "AD Cali Voltage: %d mV", voltage[0][0]);
        // }

        // 动作
        if (temp < (temperatureGoal + 0.3)) // 温度不足
        {
            if (heatingFlag != 1)
            {
                heatingFlag = 1;
                HeatingCtrl(HEAT_PWM_RATIO);
            }
        }
        else if (temp > (temperatureGoal - 0.3))
        {
            if (heatingFlag != 0)
            {
                heatingFlag = 0;
                HeatingCtrl(0);
            }
        }

        // 灯效与判定标准
        if (temp < temperatureGoal - 3)
        {
            if (ledColorRefreash != 1)
            {
                ledColorRefreash = 1;
                SetRGBLedColor(rgbLed_Heating, LED_COLOR_BLUE, LED_BLINK);
            }
            sHeatTempOk = 0;
        }
        else if (temp > (temperatureGoal - 0.3))
        {
            if (ledColorRefreash != 0)
            {
                ledColorRefreash = 0;
                SetRGBLedColor(rgbLed_Heating, LED_COLOR_GREEN, LED_NOT_BLINK);
            }
            sHeatTempOk = 1;
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
