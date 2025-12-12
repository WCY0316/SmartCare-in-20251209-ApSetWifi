#include "bsp_gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "timer.h"
#include "beep.h"
#include "main.h"

static QueueHandle_t gpio_evt_queue = NULL;
KEY_PRESS_STATE KeyState;
static const char *TAG = "GPIO";

// 外部中断处理函数
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;

    xQueueSendFromISR(gpio_evt_queue, &gpio_num, 0);
}

void InitGpio(void)
{
    gpio_config_t io_conf = {};

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    // hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void *)GPIO_INPUT_IO_0);
    gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void *)GPIO_INPUT_IO_1);
}

// GPIO外部中断处理任务
void VGpioTask(void *arg)
{
    uint32_t io_num;

    while (1)
    {
        // printf("gpio_task stack size is %ld \r\n", (int32_t)uxTaskGetStackHighWaterMark(NULL));
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {
            if (io_num == START_ENABLE_INT_PIN)
            {
                StartBuzzer(BST_CLICK);

                ResetLongPressJudging();
                ResetMultiPressJudging();
            }
            // else if (io_num == DOOR_KEY_INT_PIN) // 开盖检测
            // {
            //     printf("door opened\n");
            //     StartBuzzer(BST_ERROR);
            //     IsDoorOpen = 1;
            // }
        }
    }
}

void KeyPressStateMonitor(void)
{
    if (KeyState.PressCnt > 0) // 长按判断
    {
        if (gpio_get_level(START_ENABLE_INT_PIN) == 1)
        {
            if (--KeyState.PressCnt == 0)
            {
                ESP_LOGI(TAG, "Key Long Press");
                KeyState.BtnLongClicked = 1;
            }
        }
        else
            KeyState.PressCnt = 0;
    }

    if (KeyState.ReleaseCnt > 0) // 连续按判断，两次间隔3s内
    {
        KeyState.ReleaseCnt--;
        if (gpio_get_level(START_ENABLE_INT_PIN) == 0) // 松开
        {
            KeyState.ReleaseCnt = 0;
            KeyState.BtnSingleClicked = 1;
            KeyState.MultiPressNum++;
            if ((KeyState.MultiPressNum >= MULTI_PRESS_NUM_FOR_Start) && (GetMState() == STA_REACTING) && UserEvent.StartReacting == 0) // wcy debug
            {
                ESP_LOGI(TAG, "Key Multi clicked to start");
                KeyState.BtnDoubleClicked = 1;
            }
            if (KeyState.MultiPressNum >= MULTI_PRESS_NUM_FOR_DEBUG)
            {
                ESP_LOGI(TAG, "Key Multi clicked to debug");
                KeyState.BtnMultiClicked = 1;
            }
            printf("MultiPressNum=%ld\n", KeyState.MultiPressNum);
        }
    }
    if (KeyState.NextPressCnt > 0) // 下一次来否
    {
        if (--KeyState.NextPressCnt == 0)
        {
            KeyState.MultiPressNum = 0;
            KeyState.BtnSingleClicked = 0; // wcy debug2025.4.1
            KeyState.BtnDoubleClicked = 0;
            KeyState.BtnMultiClicked = 0;
        }
    }
}