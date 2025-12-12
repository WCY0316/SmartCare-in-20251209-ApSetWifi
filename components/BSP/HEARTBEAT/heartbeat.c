#include "heartbeat.h"
#include "tcpclient.h"

static const char *TAG = "Heartbeat";
static TimerHandle_t s_heartbeat_timer = NULL;
// static int s_tcp_socket = -1;  // TCP 套接字（需在外部初始化）
extern volatile char deviceSN[11];
// extern int SendHeartBeat(void);
// 心跳定时器回调函数
static void heartbeat_timer_callback(TimerHandle_t timer)
{
    // if (IsServerConnected() == 0)
    // {
    //     ESP_LOGW(TAG, "TCP socket not initialized");
    //     return;
    // }

    // 发送心跳消息
    int send_len = SendHeartBeat();
    if (send_len < 0)
    {
        ESP_LOGE(TAG, "Heartbeat send failed: %d", errno);
        // 可在此添加重连逻辑
    }
    else
    {
        ESP_LOGI(TAG, "Heartbeat sent successfully");
    }
    // ESP_LOGI(TAG, "Free heap: %d bytes", xPortGetFreeHeapSize());
}

/**
 * @brief 初始化心跳定时器
 * @param socket       TCP 套接字
 * @param interval_ms  心跳间隔（毫秒）
 * @return 成功返回 pdPASS，失败返回 pdFAIL
 */
BaseType_t InitHeartbeatTim(uint32_t interval_s)
{
    const esp_timer_create_args_t s_heartbeat_timer = {
        .callback = &heartbeat_timer_callback,
        /* name is optional, but may help identify the timer when debugging */
        .name = "periodic"};
    esp_timer_handle_t heartbeat_timer;
    ESP_ERROR_CHECK(esp_timer_create(&s_heartbeat_timer, &heartbeat_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(heartbeat_timer, 1000 * 1000 * interval_s)); // 微秒
    return pdPASS;
}
/**
 * @brief 停止心跳定时器
 */
void heartbeat_deinit()
{
    if (s_heartbeat_timer != NULL)
    {
        xTimerStop(s_heartbeat_timer, 0);
        xTimerDelete(s_heartbeat_timer, 0);
        s_heartbeat_timer = NULL;
        // s_tcp_socket = -1;
    }
}