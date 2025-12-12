#include "timer.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp_gpio.h"
#include "beep.h"
#include "bsp_led.h"
                                  
COUNT_DOWN_STRUCT Cd;
USER_EVENT UserEvent;
static const char *TAG = "TIMER"; // 日志打印标识

volatile uint8_t react_time_min = 2; //检测时间，默认10分钟，由服务器下发
// volatile uint8_t react_time_min = 2;
// 10ms定時器回调函数
static void periodic_timer_callback(void *arg)
{
    if (Cd.ReactingTime > 0) // 反应计时                                        
    {
        Cd.ReactingTime--;
        if (Cd.ReactingTime == 0)
        {
            UserEvent.ReactFinishFlag = 1;
        }
    }
    if (Cd.AutoHeatTime > 0) // 退出加热倒计时
    {
        Cd.AutoHeatTime--;
        if (Cd.AutoHeatTime == 0)
            UserEvent.StopAutoHeating = 1;
    }

    if (Cd.WiFiLinkTime > 0) // wifi连接倒计时
        Cd.WiFiLinkTime--;

    if (Cd.TcpLinkTime > 0) // TCP连接倒计时
        Cd.TcpLinkTime--;

    KeyPressStateMonitor();

    // 蜂鸣器
    if (BuzzerEnable)
    {
        if (++BuzzerCnt >= BUZZER_TIME)
        {
            BuzzerEnable = 0;
            BuzzerCnt = 0;
            // printf("Buzzer off\n");
            StopBuzzer();
        }
    }

    // led闪烁
    if (++blinkCnt >= RGB_BLINK_TIME)
    {
        blinkCnt = 0;
        flipFlag ^= 1;

        for (int i = 0; i < rgbLed_Index_Max; i++)
        {
            pConfig = &ColorLedConfig[i];
            if (pConfig->BlinkFlag)
            {
                if (flipFlag)
                    aw9523_set_rgb(i, pConfig->R, pConfig->G, pConfig->B);
                else
                    aw9523_set_rgb(i, 0, 0, 0);
            }
        }
    }

    for (int i = 0; i < rgbLed_Index_Max; i++)
    {
        pConfig = &ColorLedConfig[i];
        if (pConfig->NeedAction == 1) // 长亮长灭处理
        {
            pConfig->NeedAction = 0;
            aw9523_set_rgb(i, pConfig->R, pConfig->G, pConfig->B);
        }
    }
}

void InitTimer(void)
{
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &periodic_timer_callback,
        /* name is optional, but may help identify the timer when debugging */
        .name = "periodic"};
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 10 * 1000)); // 微秒
}

void StartReactCountDown(uint32_t reactMin)
{
    if (Cd.ReactingTime == 0)
    {
        Cd.ReactingTime = CONVERT_TO_SECOND(reactMin * 60);
        
        ESP_LOGI(TAG, "Start React!\n");
    }
    else
        ESP_LOGE(TAG, "Reacting now, forbid changing time!\n");
}
