#include "tcpserver.h"
#include "main.h"
#include "heat.h"
#include "tcpclient.h"
#include "bsp_log.h"

#define PORT 8888 // TCP服务器端口
#define BUFFER_SIZE 1024

static const char *TAG = "TCP_Server"; // 日志打印标识
static int client_sock = -1;

// TCP服务器任务
void VTcpServerTask(void *pvParameters)
{
    char rx_buffer[BUFFER_SIZE];
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;

    // 创建socket
    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0)
    {
        ESP_LOGE(TAG, "creat socket faild: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    // 设置端口复用
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 绑定地址
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)};
    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
    {
        ESP_LOGE(TAG, "socket bind failed : %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    // 开始监听
    if (listen(listen_sock, 5) != 0)
    {
        ESP_LOGE(TAG, "socket bind failed : errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "tcpserver already start, listening port: %d", PORT);

    while (1)
    {
        // 接受客户端连接
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &addr_len);
        if (client_sock < 0)
        {
            ESP_LOGE(TAG, "tcpserver: accept failed");
            continue;
        }

        ESP_LOGI(TAG, "tcpserver: client IP %s", inet_ntoa(client_addr.sin_addr));
        vTaskDelay(pdMS_TO_TICKS(1500));
        // 向客户端发送SN码“SN Code: SC25030001”
        char sn_code[32] = {0};
        snprintf(sn_code, sizeof(sn_code), "SN Code:%s", deviceSN);
        send(client_sock, sn_code, strlen(sn_code), 0); // 回复客户端
        ESP_LOGI(TAG, "sn code: %s", sn_code);
        vTaskDelay(pdMS_TO_TICKS(100));
        // 向客户端发送当前温度volatile float temperatureGoal = 46.0;
        char current_temp[32] = {0};
        snprintf(current_temp, sizeof(current_temp), "Now Temp:%.1f", temperatureGoal);
        send(client_sock, current_temp, strlen(current_temp), 0); // 回复客户端
        ESP_LOGI(TAG, "now temp: %s", current_temp);
        vTaskDelay(pdMS_TO_TICKS(100));
        // 向客户端发送当前域名和端口volatile char *jyrHostDnsName = "animalsocket.jyrchina.cn"; volatile int jyrPort = 20002;
        char current_dns[64] = {0};
        snprintf(current_dns, sizeof(current_dns), "Now Domin:%s", jyrHostDnsName);
        send(client_sock, current_dns, strlen(current_dns), 0); // 回复客户端
        ESP_LOGI(TAG, "now domin: %s", current_dns);
        vTaskDelay(pdMS_TO_TICKS(100));
        char current_port[32] = {0};
        snprintf(current_port, sizeof(current_port), "Now Port:%d", jyrPort);
        send(client_sock, current_port, strlen(current_port), 0); // 回复客户端
        ESP_LOGI(TAG, "now port: %s", current_port);



        // 接收数据循环
        do
        {
            int len = recv(client_sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            if (len < 0)
            {
                ESP_LOGE(TAG, "rev err: errno %d", errno);
                break;
            }
            else if (len == 0)
            {
                ESP_LOGI(TAG, "tcp client closed");
                break;
            }

            // 打印接收数据
            rx_buffer[len] = '\0';
            ESP_LOGI(TAG, "rev %d bytes data: %s", len, rx_buffer);

            char *colon_pos = DisposeStr("Now Set Temp:", rx_buffer);
            if (colon_pos != NULL)
            {
                temperatureGoal = atof(colon_pos);
                ESP_LOGI(TAG, "Set Temp : %.2f", temperatureGoal);
                write_log("INFO", TAG, "Set Temp : %.2f", temperatureGoal);
                send(client_sock, "Set Temp OK", 12, 0); // 回复客户端
                nvs_set_str(nvsHandle, "TempGoal", colon_pos);
                nvs_commit(nvsHandle);
            }

            colon_pos = DisposeStr("Now Set Domin:", rx_buffer);
            if (colon_pos != NULL)
            {
                jyrHostDnsName = strdup(colon_pos); // 自动分配内存并复制内容
                ESP_LOGI(TAG, "Set jyrHostDnsName : %s", jyrHostDnsName);
                write_log("INFO", TAG, "Set jyrHostDnsName : %s", jyrHostDnsName);
                send(client_sock, "Set Domin OK", 13, 0); // 回复客户端
                nvs_set_str(nvsHandle, "jyrHostDnsName", jyrHostDnsName);
                nvs_commit(nvsHandle);
            }

            colon_pos = DisposeStr("Now Set Port:", rx_buffer);
            if (colon_pos != NULL)
            {
                jyrPort = atoi(colon_pos);
                ESP_LOGI(TAG, "Set jyrPort : %d", jyrPort);
                write_log("INFO", TAG, "Set jyrPort : %d", jyrPort);
                send(client_sock, "Set Port OK", 12, 0); // 回复客户端
                nvs_set_str(nvsHandle, "jyrPort", colon_pos);
                nvs_commit(nvsHandle);
            }

            if (strstr(rx_buffer, "refresh") != NULL)
            {
                // 向客户端发送当前温度
                char current_temp[32] = {0};
                snprintf(current_temp, sizeof(current_temp), "Now Temp:%.1f", temperatureGoal);
                send(client_sock, current_temp, strlen(current_temp), 0);
                ESP_LOGI(TAG, "now temp: %s", current_temp);
                vTaskDelay(pdMS_TO_TICKS(100));
                // 向客户端发送当前域名和端口
                char current_dns[64] = {0};
                snprintf(current_dns, sizeof(current_dns), "Now Domin:%s", jyrHostDnsName);
                send(client_sock, current_dns, strlen(current_dns), 0);
                ESP_LOGI(TAG, "now domin: %s", current_dns);
                vTaskDelay(pdMS_TO_TICKS(100));
                char current_port[32] = {0};
                snprintf(current_port, sizeof(current_port), "Now Port:%d", jyrPort);
                send(client_sock, current_port, strlen(current_port), 0);
                ESP_LOGI(TAG, "now port: %s", current_port);
            }
            else if (strstr((const char *)rx_buffer, "Clear all log") != NULL)
            {
                ESP_LOGI(TAG, "Clear all log");
                // write_log("INFO", TAG, "Clear all log");
                clear_all_log_files();
                send(client_sock, "Clear all log OK", 16, 0); // 回复客户端
            }
            else if (strstr((const char *)rx_buffer, "Get log file sizes") != NULL)
            {
                ESP_LOGI(TAG, "Get log file sizes");
                send_log_file_sizes(); // 发送所有日志文件大小
                send(client_sock, "Get log file sizes OK", 21, 0); // 回复客户端
            }
            else if (strstr((const char *)rx_buffer, "Get pic") != NULL)
            {
                ESP_LOGI(TAG, "Get pic");
                send_picbuf(); // 发送缓存图片
                send(client_sock, "Get pic OK", 11, 0); // 回复客户端
            }


            char *colon_pos1 = DisposeStr("Get log index:", rx_buffer);
            if (colon_pos1 != NULL)
            {
                int index = atoi(colon_pos1);
                ESP_LOGI(TAG, "Get log index: %d", index);
                print_log_file_tcp(index);
            }

        } while (1);

        close(client_sock);
    }

    close(listen_sock);
    vTaskDelete(NULL);
}

int TcpSendServer(uint8_t *src, const uint32_t len)
{
    int err = send(client_sock, src, len, 0);
    if (err < 0)
    {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        return -1;
    }
    else
        printf("Tx Len=%ld\n", len);

    ESP_LOGI(TAG, "TCP send OK");

    return ESP_OK;
}