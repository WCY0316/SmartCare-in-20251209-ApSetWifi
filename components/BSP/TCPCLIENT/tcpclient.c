#include "tcpclient.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/usb_serial_jtag.h"
#include "wifi_ble_provisioning.h"
#include "bsp_led.h"
#include "main.h"
#include "camera.h"
#include "bsp_log.h"

static const char *TAG = "TCP_Client";
static uint32_t ServerConnected = 0;
int mysocket = -1;
// 主动断开tcp连接标志
volatile bool sDisconnect = 0;

SemaphoreHandle_t s_socket_mutex;

static uint8_t sTypeTestEnable = 0; // 河北型检使能标志

volatile char *jyrHostDnsName = "animalsocket.jyrchina.cn"; // 默认
volatile int jyrPort = 20002;

// 获取tcp连接状态
int IsServerConnected(void)
{
    return (ServerConnected ? 1 : 0);
}

// 关闭tcp连接
int TcpClose(void)
{
    if (mysocket != -1)
    {
        ESP_LOGE(TAG, "Shutting down socket!");
        shutdown(mysocket, 0);
        close(mysocket);
        mysocket = -1;
        // SetRGBLedColor(rgbLed_Wifi, LED_COLOR_BLUE, LED_BLINK); // 灯的状态不变
        // break; // wcy debug 3.25
    }
    return ESP_OK;
}
// 主动断开tcp连接标志清零
int TcpCloseFlag(void)
{
    sDisconnect = 0; // 主动断开tcp连接标志
    return ESP_OK;
}

int TcpSend(uint8_t *src, const uint32_t len)
{
    if (xSemaphoreTake(s_socket_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        ESP_LOGW(TAG, "Failed to acquire socket mutex");
        return ESP_FAILMUTEX;
    }

    if (ServerConnected != 1)
    {
        ESP_LOGE(TAG, "socket NOT connected!");
        xSemaphoreGive(s_socket_mutex);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "TCP ready to send");
    // write_log("INFO", TAG, "TCP ready to send");
    int err = send(mysocket, src, len, 0);
    if (err < 0)
    {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        ServerConnected = 0;
        xSemaphoreGive(s_socket_mutex);
        return ESP_FAIL;
    }
    else
        printf("Tx Len=%ld\n", len);

    ESP_LOGI(TAG, "TCP send OK");
    write_log("INFO", TAG, "send OK");

    xSemaphoreGive(s_socket_mutex);
    return ESP_OK;
}
// TCP分包发送数据
int TcpSend_fen(uint8_t *data, const uint32_t data_len)
{
    const int send_buffer_size = 1024; // 发送缓冲区大小
    int offset = 0;                    // 已发送数据的偏移量

    while (offset < data_len)
    {
        // 计算本次发送的数据长度，不超过发送缓冲区大小
        int bytes_to_send = (data_len - offset) > send_buffer_size ? send_buffer_size : (data_len - offset);

        // 发送数据
        int bytes_sent = send(mysocket, data + offset, bytes_to_send, 0);

        if (bytes_sent == -1)
        {
            // 发送失败，处理错误
            perror("send failed");
            return ESP_FAIL;
        }

        // 更新已发送数据的偏移量
        offset += bytes_sent;

        // 可以在这里添加日志输出，用于调试
        printf("Sent %d bytes, total sent: %d\n", bytes_sent, offset);
    }

    printf("All data sent successfully\n");
    return ESP_OK;
}
void VTcpClientTask(void *pvParameters)
{
    char rx_buffer[256];
    memset(rx_buffer, 0, sizeof(rx_buffer));
    // char host_ip[] = HOST_IP_ADDR;
    int addr_family = 0;
    int ip_protocol = 0;
    int once = 1;
    int i;
    struct addrinfo hints;
    struct addrinfo *res;
    struct in_addr *addr;
    int err;
    static bool temp_once = false; // 用于第一次连接时

    // 互斥量
    s_socket_mutex = xSemaphoreCreateMutex();
    if (!s_socket_mutex)
    {
        ESP_LOGE(TAG, "Mutex creation failed");
        // return pdFAIL;
    }
    ESP_LOGE(TAG, "Socket mutex created successfully");
    // xSemaphoreGive(s_socket_mutex);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // For IPv4
    hints.ai_socktype = SOCK_STREAM;

    ESP_LOGI(TAG, "Resolving %s", jyrHostDnsName);

    // 定义储存服务器ip的变量 wcy debug 3.27
    static char server_ip[16];

    while (1)
    {
        // char *temp = jyrHostDnsName;
        err = getaddrinfo(jyrHostDnsName, NULL, &hints, &res);

        if (err != 0 || res == NULL)
        {
            // ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        ESP_LOGI(TAG, "DNS lookup2 succeeded. IP=%s", inet_ntoa(*addr));
        memcpy(server_ip, inet_ntoa(*addr), 16);
        ESP_LOGI(TAG, "Server IP=%s", server_ip);
        freeaddrinfo(res);
        // addr.s_addr = inet_addr(HOST_IP_ADDR);
        while (1)
        {
            while (!IsWifiConnected())
            {
                if (mysocket != -1)
                {
                    ESP_LOGE(TAG, "Shutting down socket and restarting...");
                    shutdown(mysocket, 0);
                    close(mysocket);
                    mysocket = -1;
                }
                vTaskDelay(1000);
            }
            // 主动断开时，不自动重连
            if (sDisconnect == 1)
            {
                // sDisconnect = 0;
                vTaskDelay(1000);
                continue;
            }
            // memcpy(server_ip, "192.168.50.227", strlen("192.168.50.227") + 1);
            
            if (once == 1)
            {
                once = 0;
                ESP_LOGI(TAG, "Start tcp client to %s:%d", server_ip, jyrPort);
                // ESP_LOGI(TAG, "Server IP=%s", inet_ntoa(*addr));
            }
#if defined(CONFIG_EXAMPLE_IPV4)
            struct sockaddr_in dest_addr;
#if 0
			inet_pton(AF_INET, host_ip, &dest_addr.sin_addr);
#else
            // dest_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*addr));
            dest_addr.sin_addr.s_addr = inet_addr(server_ip); // wcy debug 3.27
#endif
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(jyrPort);
            addr_family = AF_INET;
            ip_protocol = IPPROTO_IP;
#elif defined(CONFIG_EXAMPLE_SOCKET_IP_INPUT_STDIN)
            struct sockaddr_storage dest_addr = {0};
            ESP_ERROR_CHECK(get_addr_from_stdin(PORT, SOCK_STREAM, &ip_protocol, &addr_family, &dest_addr));
#endif

            mysocket = socket(addr_family, SOCK_STREAM, ip_protocol);
            if (mysocket < 0)
            {
                ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
                break;
            }
            // ESP_LOGI(TAG, "Socket created, connecting to %s:%d", host_ip, PORT);

            int err = connect(mysocket, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            if (err != 0)
            {
                ESP_LOGE(TAG, "Socket unable to connect: errno %d,err=%d", errno, err);
                break;
            }
            ESP_LOGI(TAG, "Successfully connected~~");
            SetRGBLedColor(rgbLed_Wifi, LED_COLOR_GREEN, LED_NOT_BLINK);
            once = 1;
            temp_once = true;
            ServerConnected = 1;
            CheckJpgCaching(); // 检查是否有上次断网后缓存的JPG

            while (1)
            {
                if (!IsWifiConnected())
                    break;
                if (ServerConnected == 0)
                    break;

                int len = recv(mysocket, rx_buffer, sizeof(rx_buffer) - 1, 0);
                // Error occurred during receiving
                if (len < 0)
                {
                    ESP_LOGE(TAG, "recv failed: errno %d", errno);
                    break;
                }
                // Data received
                else if (len > 0)
                {
                    ESP_LOGI(TAG, "Received %d bytes : %s:", len, rx_buffer);
                    char *colon_pos = DisposeStr("Now Set React Time:", rx_buffer);
                    if(colon_pos != NULL)
                    {
                        react_time_min = atoi(colon_pos);
                        ESP_LOGI(TAG, "Set React Time1 : %d", react_time_min);
                        write_log("INFO", TAG, "Set React Time : %d", react_time_min);
                    }
                    // rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                    // ESP_LOGI(TAG, "Received %d bytes from %s:", len, host_ip);
                    // printf("%d\n",len);
                    // ESP_LOGI(TAG, "%s", rx_buffer);
                    memset(rx_buffer, 0, sizeof(rx_buffer)); // 清空接收缓存
                    len = 0;
                }
                vTaskDelay(pdMS_TO_TICKS(5));
            }
            ServerConnected = 0;

            if (mysocket != -1)
            {
                ESP_LOGE(TAG, "Shutting down socket and restarting...");
                shutdown(mysocket, 0);
                close(mysocket);
                mysocket = -1;
                printf("sDisconnect:%d\n", sDisconnect);
                if (!sDisconnect && temp_once) // 主动断开tcp连接时，不闪烁
                {
                    SetRGBLedColor(rgbLed_Wifi, LED_COLOR_BLUE, LED_BLINK);
                }

                // break; // wcy debug 3.25
            }
        }
        vTaskDelay(1000);
    }
}

// 上传开始时间至服务器
int SendStartTimeToServer(void)
{
    uint8_t len, buf[32];
    // sUploadPicSuccess = 0; // 上传开始时间成功标志位

    printf("send start timer to server!\n");
    memset(buf, 0, sizeof(buf));
    len = 0;
    buf[len++] = 0xAA;
    buf[len++] = 0xBB;
    len += 2;
    buf[len++] = 0xFF;
    buf[len++] = 0xFF;
    buf[len++] = 0xFF;
    buf[len++] = 0xFF;

    buf[len++] = 0xF2;
    memcpy(buf + len, deviceSN, sizeof(deviceSN) - 1);
    len += (sizeof(deviceSN) - 1);

    if (GetMState() == STA_DEBUG)
        buf[len++] = 0x01; // 调试状态
    else if (sTypeTestEnable)
        buf[len++] = 0x01; // 型检，调试状态
    else
        buf[len++] = 0x00;

    buf[len++] = 0xFF;
    buf[len++] = 0xFF;
    buf[len++] = 0xFF;
    buf[len++] = 0xFF;

    buf[len++] = 0x00;
    buf[len++] = 0x00;
    buf[len++] = 0xCC;

    buf[2] = (len - 11) & 0xFF;
    buf[3] = ((len - 11) >> 8) & 0xFF;

    printf("Send buf: ");
    for (int i = 0; i < len; i++)
    {
        printf(" %02X ", buf[i]);
    }
    printf("\n");

    int ret = TcpSend(buf, len);
    if (ret == ESP_FAIL)
    {
        printf("TcpSend starttime error: %d,len=%d\n", ret, len);
    }
    else if (ret == ESP_FAILMUTEX) // 信号量获取失败时，再重新发送
    {
        ESP_LOGE(TAG, "Time: Failed to acquire socket mutex");
        return ESP_FAILMUTEX;
    }
    // sUploadPicSuccess = 1; // 上传开始时间成功标志位

    return 1;
}
// 上传图片至服务器
int SendJpgToServer(uint8_t *PicData, uint32_t PicSize)
{
    if (PicData == NULL)
        return 0;
    if ((PicSize == 0))
    {
        printf("Jpg Size error: %ld\n", PicSize);
        return 0;
    }

    uint32_t len = 0;
    uint8_t *picNotify = (uint8_t *)malloc(PicSize + 64);
    printf("malloc %p,size=%ld\n", picNotify, PicSize + 64);

    picNotify[len++] = 0xAA;
    picNotify[len++] = 0xBB;
    len += 2;
    picNotify[len++] = 0xFF;
    picNotify[len++] = 0xFF;
    picNotify[len++] = 0xFF;
    picNotify[len++] = 0xFF;

    picNotify[len++] = 0xF1;
    memcpy(picNotify + len, deviceSN, sizeof(deviceSN) - 1);
    len += (sizeof(deviceSN) - 1);

    if (GetMState() == STA_DEBUG)
        picNotify[len++] = 0x01; // 调试状态
    else if (sTypeTestEnable)
        picNotify[len++] = 0x01; // 型检，调试状态
    else
        picNotify[len++] = 0x00;

    picNotify[len++] = 0xFF;
    picNotify[len++] = 0xFF;

    memcpy(picNotify + len, PicData, PicSize);
    len += PicSize;

    picNotify[len++] = 0x00;
    picNotify[len++] = 0x00;
    picNotify[len++] = 0xCC;

    picNotify[2] = (len - 11) & 0xFF;
    picNotify[3] = ((len - 11) >> 8) & 0xFF;

    int ret = TcpSend(picNotify, len);
    if (ret == ESP_FAIL)
    {
        printf("TcpSend error: %d, cache pic to Flash,len=%ld\n", ret, len);
        SavePicToFlash(picNotify, len);
    }
    else if (ret == ESP_FAILMUTEX)
    {
        ESP_LOGE(TAG, "Picture: Failed to acquire socket mutex");
        return ESP_FAILMUTEX;
    }
    free(picNotify);

    return 1;
}
// 上传心跳包至服务器
int SendHeartBeat(void)
{
    uint8_t len, buf[32];

    printf("send heartbeat!\n");
    memset(buf, 0, sizeof(buf));
    len = 0;
    buf[len++] = 0xAA;
    buf[len++] = 0xBB;
    len += 2;
    buf[len++] = 0xFF;
    buf[len++] = 0xFF;
    buf[len++] = 0xFF;
    buf[len++] = 0xFF;

    buf[len++] = 0xF4;
    memcpy(buf + len, deviceSN, sizeof(deviceSN) - 1);
    len += (sizeof(deviceSN) - 1);

    // if (GetMState() == STA_DEBUG)
    //     buf[len++] = 0x01; // 调试状态
    // else if (sTypeTestEnable)
    //     buf[len++] = 0x01; // 型检，调试状态
    // else
    //     buf[len++] = 0x00;
    buf[len++] = 0xFF;
    buf[len++] = 0xFF;
    buf[len++] = 0xFF;
    buf[len++] = 0xFF;
    buf[len++] = 0xFF;

    buf[len++] = 0x00;
    buf[len++] = 0x00;
    buf[len++] = 0xCC;

    buf[2] = (len - 11) & 0xFF;
    buf[3] = ((len - 11) >> 8) & 0xFF;

    printf("Send buf: ");
    for (int i = 0; i < len; i++)
    {
        printf(" %02X ", buf[i]);
    }
    printf("\n");

    int ret = TcpSend(buf, len);
    if (ret != ESP_OK)
    {
        printf("TcpSend starttime error: %d,len=%d\n", ret, len);
        return -1;
    }

    return 1;
}