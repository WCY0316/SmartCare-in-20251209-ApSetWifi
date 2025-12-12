#include "main.h"
#include "usb_com.h"
#include "heartbeat.h"
#include "ds18b20.h"
#include "esp_system.h"
#include "wifi_ble_provisioning.h"
#include "bsp_gpio.h"
#include "timer.h"
#include "beep.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "bsp_led.h"
#include "heat.h"
#include "tcpclient.h"
#include "camera.h"
#include "tcpserver.h"
#include "bsp_log.h"
#include "ap_set_wifi.h"

char deviceSN[11] = "SC00000000"; // 设备名称

MACHINE_STATE state_id = STA_INIT;

static const char *TAG = "MAIN"; // 日志打印标识

nvs_handle_t nvsHandle; // NVS句柄

TaskHandle_t tcpServerTaskHandle = NULL;

extern volatile bool sDisconnect;

// 字符串处理函数
char *DisposeStr(char *str, char *buf)
{
    if (strstr((const char *)buf, str) != NULL)
    {
        // 找到“:”的位置
        char *colon_pos = strchr((const char *)buf, ':');
        if (colon_pos != NULL)
        {
            // 截取“:”后面的内容
            char *sn_code = colon_pos + 1;
            // 去除前后的空格
            while (*sn_code == ' ')
                sn_code++;
            // 打印SN码
            ESP_LOGE(TAG, "Dispose str: %s\n", sn_code);
            return sn_code;
        }
        return NULL;
    }
    return NULL;
}

char *GetMStateStr(MACHINE_STATE state)
{
    switch (state)
    {
    case STA_INIT:
        return "STA_INIT";
    case STA_IDLE:
        return "STA_IDLE";
    case STA_BLE:
        return "STA_BLE";
    case STA_HEATING:
        return "STA_HEATING";
    case STA_REACTING:
        return "STA_REACTING";
    case STA_PAUSE:
        return "STA_PAUSE";
    case STA_DEBUG:
        return "STA_DEBUG";
    case STA_ERROR:
        return "STA_ERROR";
    case STA_BACK_TO_IDLE:
        return "STA_BACK_TO_IDLE";
    default:
        return "Unknown";
    }
}
// 状态机切换
int SetMState(MACHINE_STATE newState)
{
    printf("--------------------SM: %s --> %s --------------------\n", GetMStateStr(state_id), GetMStateStr(newState));
    write_log("INFO", TAG, "State change: %s --> %s", GetMStateStr(state_id), GetMStateStr(newState));
    state_id = newState;

    // if (newState == STA_ERROR)
    //     StartBuzzer(BST_ERROR);

    return ((state_id == newState) ? 1 : 0);
}
int GetMState(void)
{
    return state_id;
}

void ShowVersion(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGE(TAG, "This is %s chip with %d CPU core(s), %s%s%s%s, ",
             CONFIG_IDF_TARGET,
             chip_info.cores,
             (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
             (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
             (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
             (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;

    ESP_LOGE(TAG, "silicon revision v%d.%d, ", major_rev, minor_rev);
    ESP_LOGE(TAG, "--------------------------------------------------\n");
    ESP_LOGE(TAG, "--------------SmartCare by ZSJY-------------------\n");
    ESP_LOGE(TAG, "--------------Firmware Version:%s--------------\n", FIRMWARE_VERSION);
    ESP_LOGE(TAG, "--------------SN Code:%s-----------------\n", deviceSN);
    ESP_LOGE(TAG, "--------------------------------------------------\n");
}

void VRoutineTask(void *arg)
{
    uint8_t once_blufi = 0;
    uint32_t num = 1;
    static uint8_t isFirstSetPara = 0; // 是否第一次设置参数
    static bool once_debug = false;    // 是否第一次进入debug模式

    Cd.WiFiLinkTime = WIFI_LINK_MAX_SECOND; // 联网倒计时
    // Cd.TcpLinkTime = TCP_LINK_MAX_SECOND;   // TCP连接倒计时
    // write_log("INFO", TAG, "Routine Task Start!");

    SetMState(STA_INIT);
    while (1)
    {
        switch (state_id)
        {
        case STA_INIT:
            if (IsWifiConnected() && IsServerConnected())
            {
                Cd.WiFiLinkTime = 0;
                KeyState.BtnSingleClicked = 0;
                KeyState.BtnLongClicked = 0;
                SetMState(STA_IDLE);
                write_log("INFO", TAG, "Wifi Connected!");
            }
            else
            {
                if (Cd.WiFiLinkTime == 0)
                {
                    ESP_LOGI(TAG, "Wifi Timeout!");
                    write_log("WARN", TAG, "Wifi Timeout!");
                    SetRGBLedColor(rgbLed_Wifi, LED_COLOR_RED, LED_NOT_BLINK);
                    StartBuzzer(BST_ERROR);
                    SetMState(STA_ERROR);
                }
            }
            break;
        case STA_IDLE:

            if (KeyState.BtnSingleClicked == 1) // 短按
            {
                KeyState.BtnSingleClicked = 0;
                AutoHeatEnable = 1;
                SetRGBLedColor(rgbLed_Heating, LED_COLOR_BLUE, LED_BLINK); // wcy debug2025.4.1
                sDisconnect = 0;
                TcpCloseFlag(); // 开启自动重连  wcy debug2025.4.15
                // once_debug = true;                                         // 进入加热状态后，不允许进入调试模式
                SetMState(STA_HEATING);
            }
            else if (KeyState.BtnLongClicked) // 长按
            {
                KeyState.BtnLongClicked = 0;
                SetMState(STA_BLE);
            }

            if (!IsWifiConnected()) // 若WIFI断开，重新进入STA_INIT状态
            {
                SetMState(STA_INIT);
                write_log("INFO", TAG, "Wifi disconnect!");
            }

            // else if (KeyState.BtnMultiClicked)      //连击
            // {
            //     KeyState.BtnMultiClicked = 0;
            //     SetMState(STA_DEBUG);
            // }
            break;
        case STA_BLE:
            if (once_blufi == 0) // 每次上电只进一次
            {
                StartBuzzer(BST_SUCC);
                SetRGBLedColor(rgbLed_Wifi, LED_COLOR_BLUE, LED_BLINK);
                ResetWiFiLinkState();
                write_log("INFO", TAG, "Start Blufi!");
                // blufi_entry_func(deviceSN);
                once_blufi = 1;
            }
            if (IsWifiConnected() && IsServerConnected())
            {
                SetMState(STA_INIT);
                write_log("INFO", TAG, "Blufi network success!");
            }

            break;
        case STA_HEATING:
            if (IsTemperatureOK() && KeyState.MultiPressNum == 0)
            {

                UserEvent.StartReacting = 0;
                KeyState.BtnSingleClicked = 0;
                Cd.AutoHeatTime = STOP_HEATING_COUNTDOWN; // 温度达标后一段时间内无任何操作，则退出
                SetMState(STA_REACTING);
                // write_log("INFO", TAG, "Temperature OK!");
            }

            if (KeyState.BtnMultiClicked && once_debug == false) // 连击
            {
                KeyState.BtnMultiClicked = 0;
#if HEBEI_TYPE_TEST
                ESP_LOGI(TAG, "HeBei Type Test Mode!\n");
                sTypeTestEnable = 1;
#else
                // AutoHeatEnable = 0;

                SetMState(STA_DEBUG);
                // SetRGBLedColor(rgbLed_Heating, LED_COLOR_BLUE, LED_NOT_BLINK);
#endif
                SetRGBLedColor(rgbLed_Reacting, LED_COLOR_BLUE, LED_NOT_BLINK);
                write_log("INFO", TAG, "Debug Mode!");
            }
            break;
        case STA_REACTING:
            if (UserEvent.StartReacting == 0)
            {
                if (KeyState.BtnDoubleClicked == 1) // wcy debug
                {
                    KeyState.BtnDoubleClicked = 0;
                    if (IsWifiConnected() && IsServerConnected()) // 网络状态良好时，才允许进行反应
                    {
#if HEBEI_TYPE_TEST
                        StartReactCountDown(1);
#else
                        StartReactCountDown(react_time_min); // 开始反应计时, 单位为分钟
                        ESP_LOGI(TAG, "react time:%d", react_time_min);
                        write_log("INFO", TAG, "react time:%d", react_time_min);
#endif
                        while (SendStartTimeToServer() == ESP_FAILMUTEX) // 将开始时间同步给服务器
                        {
                            ESP_LOGE(TAG, "Failed to acquire socket mutex");
                            vTaskDelay(pdMS_TO_TICKS(5000));
                        }
                        SetRGBLedColor(rgbLed_Reacting, LED_COLOR_BLUE, LED_BLINK);
                        ESP_LOGI(TAG, "Start Reacting!\n");
                        write_log("INFO", TAG, "Start Reacting!");

                        UserEvent.StartReacting = 1;
                        Cd.AutoHeatTime = 0; // 关闭加热退出机制
                    }
                    else
                    {
                        ESP_LOGI(TAG, "Wifi not linked, refuse to react!");
                        SetRGBLedColor(rgbLed_Wifi, LED_COLOR_RED, LED_NOT_BLINK);
                        SetMState(STA_ERROR);
                        write_log("ERROR", TAG, "Wifi not linked, refuse to react!");
                    }
                }
            }
            else
            {
                if (UserEvent.ReactFinishFlag == 1)
                {
                    UserEvent.ReactFinishFlag = 0;
#if HEBEI_TYPE_TEST
                    StartReactCountDown(CONVERT_TO_SECOND(30)); // 河北型检，希望进入型检状态后每30s自动拍照一次
#else
                    UserEvent.StartReacting = 0;
#endif
                    // xQueueSend(camera_evt_queue, &num, 0);
                    TakePic();
                    StartBuzzer(BST_SUCC);
                    SetRGBLedColor(rgbLed_Reacting, LED_COLOR_GREEN, LED_NOT_BLINK);
                    Cd.AutoHeatTime = STOP_HEATING_COUNTDOWN;
                    write_log("INFO", TAG, "Reacting Finish!");
                }

                // if (IsDoorOpen == 1) // 若检测到开盖
                // {
                //     StartBuzzer(BST_ERROR);
                //     SetRGBLedColor(rgbLed_Reacting, LED_COLOR_RED, LED_NOT_BLINK);
                //     Cd.ReactTimeFreeze = Cd.ReactingTime; // 暂存剩余反应时间，并暂停反应
                //     Cd.ReactingTime = 0;
                //     SetMState(STA_PAUSE);
                // }
            }

            if (UserEvent.StopAutoHeating) // 30min内无操作，退出加热模式
            {
                UserEvent.StopAutoHeating = 0;
                sDisconnect = 1;
                TcpClose(); // 主动关闭TCP连接
                SetMState(STA_BACK_TO_IDLE);
                write_log("INFO", TAG, "Auto Heating stop and enter standby mode!");
                SetRGBLedColor(rgbLed_Wifi, LED_COLOR_GREEN, LED_NOT_BLINK);
            }
            break;
        case STA_PAUSE:
            // if (gpio_get_level(DOOR_KEY_INT_PIN) == 0) // 若检测到合盖
            // {
            //     Cd.ReactingTime = Cd.ReactTimeFreeze;
            //     SetRGBLedColor(rgbLed_Reacting, LED_COLOR_BLUE, LED_BLINK);
            //     ESP_LOGI(TAG, "continue Reacting!\n");
            //     SetMState(STA_REACTING);
            //     IsDoorOpen = 0; // wcy debug 20250226
            // }
            break;

        case STA_BACK_TO_IDLE:
            AutoHeatEnable = 0;
            SetHeatTempOK(0); // wcy debug2025.4.1
            SetMState(STA_IDLE);
            break;
        case STA_DEBUG:
            if (KeyState.BtnSingleClicked == 1) // 短按
            {
                KeyState.BtnSingleClicked = 0;
                TakePic();
                static uint8_t i = 0;
                ESP_LOGI(TAG, "debug %d!\n", ++i);
            }
            if (isFirstSetPara == 0)
            {
                StartAp();                                                                             // 启动AP模式
                xTaskCreate(VTcpServerTask, "TcpServerTask", 1024 * 6, NULL, 6, &tcpServerTaskHandle); // 创建TCP服务器任务
                isFirstSetPara = 1;
                // write_log("INFO", TAG, "Start AP mode!");
            }
            break;
        case STA_ERROR:
            if (KeyState.BtnLongClicked) // 长按
            {
                SetMState(STA_BLE);
            }
            if (IsWifiConnected())
            {
                TcpCloseFlag(); // 开启自动重连  wcy debug2025.9.12
                if (IsServerConnected())
                {
                    Cd.WiFiLinkTime = 0;
                    KeyState.BtnSingleClicked = 0;
                    SetMState(STA_IDLE);
                }
            }
            break;
        default:
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
// 读取SN码，nvs初始化
void Read_SN_Code(void)
{
    esp_err_t ret;
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ret = nvs_open("storage", NVS_READWRITE, &nvsHandle);
    if (ret != ESP_OK)
        ESP_LOGI(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(ret));

    char temp[64] = {0};
    uint8_t len = sizeof(temp);
    ret = nvs_get_str(nvsHandle, "SNCode", temp, &len);
    ESP_LOGI(TAG, "SNCode: %s", temp);
    ESP_LOGI(TAG, "SNCode len: %d", len);
    if (ret == ESP_OK)
    {
        strncpy(deviceSN, temp, sizeof(deviceSN) - 1);
        deviceSN[sizeof(deviceSN) - 1] = '\0'; // 确保字符串以'\0'结尾
        ESP_LOGI(TAG, "Device SN read from NVS: %s\n", deviceSN);
        write_log("INFO", TAG, "Device SN: %s", deviceSN);
    }
    else
        ESP_LOGI(TAG, "Error (%s) reading SNCode!\n", esp_err_to_name(ret));

    memset(temp, 0, sizeof(temp));
    len = sizeof(temp);
    ret = nvs_get_str(nvsHandle, "TempGoal", temp, &len);
    ESP_LOGI(TAG, "TempGoal: %s", temp);
    ESP_LOGI(TAG, "TempGoal len: %d", len);

    if (ret == ESP_OK)
    {
        float tempfol = atof(temp);
        temperatureGoal = tempfol;
        ESP_LOGI(TAG, "TempGoal read from NVS: %.1f\n", temperatureGoal);
        write_log("INFO", TAG, "TempGoal: %.1f", temperatureGoal);
    }
    else
        ESP_LOGI(TAG, "Error (%s) reading TempGoal!\n", esp_err_to_name(ret));

    memset(temp, 0, sizeof(temp));
    len = sizeof(temp);
    ret = nvs_get_str(nvsHandle, "jyrHostDnsName", temp, &len);
    ESP_LOGI(TAG, "jyrHostDnsName: %s", temp);
    ESP_LOGI(TAG, "jyrHostDnsName len: %d", len);
    if (ret == ESP_OK)
    {
        jyrHostDnsName = strdup(temp); // 自动分配内存并复制内容
        ESP_LOGI(TAG, "jyrHostDnsName read from NVS: %s\n", jyrHostDnsName);
        write_log("INFO", TAG, "jyrHostDnsName: %s", jyrHostDnsName);
    }
    else
        ESP_LOGI(TAG, "Error (%s) reading jyrHostDnsName!\n", esp_err_to_name(ret));

    memset(temp, 0, sizeof(temp));
    len = sizeof(temp);
    ret = nvs_get_str(nvsHandle, "jyrPort", temp, &len);
    ESP_LOGI(TAG, "jyrPort: %s", temp);
    ESP_LOGI(TAG, "jyrPort len: %d", len);

    if (ret == ESP_OK)
    {
        jyrPort = atoi(temp);
        ESP_LOGI(TAG, "jyrPort read from NVS: %d\n", jyrPort);
        write_log("INFO", TAG, "jyrPort: %d", jyrPort);
    }
    else
        ESP_LOGI(TAG, "Error (%s) reading jyrPort!\n", esp_err_to_name(ret));

    // nvs_set_str(nvsHandle, "SNCode", "SC25030001");
    // nvs_set_str(nvsHandle, "wifi_ssid", "80108963");
    // nvs_set_str(nvsHandle, "wifi_passwd", "80108963");
    // nvs_set_str(nvsHandle, "jyrPort", "20002");
    // nvs_set_str(nvsHandle, "jyrHostDnsName", "pet.010test.top");
}

// 系统级初始化（硬件+软件）
static void System_Init(void)
{
    init_log_storage(); // 初始化日志存储
    Read_SN_Code();     // 读取SN码
    ShowVersion();      // 显示版本信息
    InitGpio();         // 初始化GPIO
    InitTimer();        // 初始化定时器
    InitBuzzer();       // 初始化蜂鸣器
    InitLed();          // 初始化LED
    SelfCheck();        // 自检
    // InitUserWifi();       // 初始化WIFI
    start_ApSetWifi(); // 启动AP配置WiFi
    InitHeat();        // 初始化加热模块
    InitCamera();      // 初始化摄像头
    // InitHeartbeatTim(30); // 初始化心跳定时器
}
void app_main(void)
{
    static uint8_t isconble = 0;

    System_Init(); // 系统外设始化
    // write_log("INFO", TAG, "System Init OK!");

    // TaskHandle_t routineTaskHandle = NULL;
    // xTaskCreate(VRoutineTask, "RoutineTask", 1024 * 4, NULL, 0, &routineTaskHandle); // 状态机主任务
    // TaskHandle_t gpioTaskHandle = NULL;
    // xTaskCreate(VGpioTask, "GpioTask", 1024 * 3, NULL, 1, &gpioTaskHandle); // 创建GPIO任务
    // TaskHandle_t tcpclientTaskHandle = NULL;
    // xTaskCreate(VTcpClientTask, "TcpClientTask", 1024 * 4, NULL, 2, &tcpclientTaskHandle); // 创建TCP客户端任务
    // TaskHandle_t cameraTaskHandle = NULL;
    // xTaskCreate(VCameraTask, "CameraTask", 1024 * 4, NULL, 3, &cameraTaskHandle); // 创建摄像头任务
    TaskHandle_t heatTaskHandle = NULL;
    xTaskCreate(VHeatTask, "HeatTask", 1024 * 3, NULL, 5, &heatTaskHandle); // 创建加热任务
    TaskHandle_t usbTaskHandle = NULL;
    // xTaskCreate(VUsbTask, "UsbTask", 1024 * 4, NULL, 15, &usbTaskHandle); //  USB任务
    // TaskHandle_t timeTaskHandle = NULL;
    // xTaskCreate(VTimeTask, "TimeTask", 1024 * 3, NULL, 7, &timeTaskHandle); // 创建SNTP时间任务，获取到时间后自动删除任务

    // StartAp();                                                                             // 启动AP模式
    // xTaskCreate(VTcpServerTask, "TcpServerTask", 1024 * 6, NULL, 6, &tcpServerTaskHandle); // 创建TCP服务器任务
    int num = 1;
    // clear_all_log_files(); // 清除所有日志文件

    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        // printf("GpioTask stack size is %ld \r\n", (int32_t)uxTaskGetStackHighWaterMark(gpioTaskHandle));
        // printf("RoutineTask stack size is %ld \r\n", (int32_t)uxTaskGetStackHighWaterMark(routineTaskHandle));
        // printf("HeatTask stack size is %ld \r\n", (int32_t)uxTaskGetStackHighWaterMark(heatTaskHandle));
        // printf("VTcpClientTask stack size is %ld \r\n", (int32_t)uxTaskGetStackHighWaterMark(tcpclientTaskHandle));
        // printf("CameraTask stack size is %ld \r\n", (int32_t)uxTaskGetStackHighWaterMark(cameraTaskHandle));
        // printf("main stack size is %ld \r\n", (int32_t)uxTaskGetStackHighWaterMark(NULL));

        // 正常检测之前才可以设置SN码，设置完自动停止任务
        if (isSuccSetSN || ((GetMState() >= STA_HEATING) && (GetMState() < STA_ERROR)))
        {
            // 停止usb任务操作，设置完SN码就退出
            if (usbTaskHandle != NULL)
            {
                ESP_LOGI(TAG, "Stopping usbtask gracefully");
                // write_log("INFO", TAG, "Stopping usbtask gracefully");
                vTaskDelete(usbTaskHandle); // 安全删除任务
                usbTaskHandle = NULL;
            }
            isSuccSetSN = 0;
        }

        // 放在状态机里有时候蓝牙名称会设置失败，所以放在这里 20250415
        if (GetMState() == STA_BLE && isconble == 0)
        {
            isconble = 1;
            blufi_entry_func(deviceSN);
            // write_log("INFO", TAG, "Bluetooth network configuration started");
        }
        // write_log("INFO", TAG, "System Init OK!  %d \n", num++);
        // print_log_file(1);
    }
}