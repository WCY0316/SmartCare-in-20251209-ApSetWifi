#include "usb_com.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_partition.h"
#include "driver/usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp_log.h"

// USB通信初始化配置

extern nvs_handle_t nvsHandle;

extern char deviceSN[10];
volatile bool isSuccSetSN = 0; // 是否成功设置SN码
uint8_t usb_recv_buffer[RX_BUFFER_SIZE];
static const char *TAG = "usbPrj";
static size_t recv_index = 0;

void usb_comm_init(void)
{
    // 配置USB-JTAG-Serial控制器（默认使用GPIO19/20）
    const usb_serial_jtag_driver_config_t usb_config = {
        // .tx_io_num = GPIO_NUM_20,  // DP引脚
        // .rx_io_num = GPIO_NUM_19,  // DN引脚
        .tx_buffer_size = 512,
        .rx_buffer_size = 512};
    usb_serial_jtag_driver_install(&usb_config);
}

// 发送数据
void usb_send_data(const char *data, size_t len)
{
    usb_serial_jtag_write_bytes((const uint8_t *)data, len, pdMS_TO_TICKS(1000));
}

void usb_receive_data(void)
{
    // // 非阻塞接收数据
    uint8_t len = 0;
    len = usb_serial_jtag_read_bytes(usb_recv_buffer + recv_index, RX_BUFFER_SIZE - recv_index, 0);

    if (len > 0)
    {
        recv_index += len;

        // 查找换行符
        size_t line_end = 0;
        while (line_end < recv_index)
        {
            if (usb_recv_buffer[line_end] == '\n')
            {
                // 打印到换行符的内容（包含\n）
                ESP_LOGE(TAG, "%.*s", (int)(line_end + 1), usb_recv_buffer);
                // 判断字符串中是否包含特定的字符串，并截取特定字符串后面的内容
                if (strstr((const char *)usb_recv_buffer, "Now Set SNCode:") != NULL)
                {
                    // 找到“:”的位置
                    char *colon_pos = strchr((const char *)usb_recv_buffer, ':');
                    if (colon_pos != NULL)
                    {
                        // 截取“:”后面的内容
                        char *sn_code = colon_pos + 1;
                        // 去除前后的空格
                        while (*sn_code == ' ')
                            sn_code++;
                        // 打印SN码
                        ESP_LOGE(TAG, "SN Code: %s\n", sn_code);
                        // 赋值给全局变量
                        memset(deviceSN, 0, sizeof(deviceSN));
                        // strncpy(deviceSN, sn_code, sizeof(deviceSN));
                        snprintf(deviceSN, sizeof(deviceSN) + 1, "%s", sn_code);
                        // deviceSN[sizeof(deviceSN) - 1] = '\0'; // 确保字符串以'\0'结尾
                        ESP_LOGE(TAG, "Device SN: %s\n", deviceSN);
                        write_log("INFO", TAG, "Set SNCode: %s", deviceSN);
                        // 回复数据
                        const char *reply = "Set SNCode OK\n";
                        usb_send_data(reply, strlen(reply));
                        // 存储到NVS
                        nvs_set_str(nvsHandle, "SNCode", deviceSN);
                        nvs_commit(nvsHandle);
                        isSuccSetSN = 1; // 设置成功
                    }
                }
                else if (strstr((const char *)usb_recv_buffer, "Get log index:") != NULL)
                {
                    // 找到“:”的位置
                    char *colon_pos = strchr((const char *)usb_recv_buffer, ':');
                    if (colon_pos != NULL)
                    {
                        // 截取“:”后面的内容
                        char *log_index = colon_pos + 1;
                        // 去除前后的空格
                        while (*log_index == ' ')
                            log_index++;
                        // ESP_LOGI(TAG, "Log index: %s\n", log_index);
                        int index = atoi(log_index);
                        print_log_file(index);
                    }
                }
                else if (strstr((const char *)usb_recv_buffer, "Clear all log") != NULL)
                {
                    ESP_LOGI(TAG, "Clear all log files\n");
                    // write_log("INFO", TAG, "Clear all log files");
                    clear_all_log_files(); // 清除所有日志文件
                }
                else if (strstr((const char *)usb_recv_buffer, "Get log file sizes") != NULL)
                {
                    print_all_log_file_sizes(); // 打印所有日志文件大小
                }

                // 移动剩余数据到缓冲区头部
                size_t remaining = recv_index - (line_end + 1);
                memmove(usb_recv_buffer, usb_recv_buffer + line_end + 1, remaining);
                recv_index = remaining;
                line_end = 0; // 重置检查位置
            }
            else
            {
                line_end++;
            }
        }

        // 处理缓冲区溢出
        if (recv_index >= RX_BUFFER_SIZE)
        {
            ESP_LOGE(TAG, "Buffer overflow, resetting index\n");
            recv_index = 0;
        }
    }
}
void VUsbTask(void *arg)
{
    usb_comm_init();

    // memset(usb_recv_buf, 0, sizeof(usb_recv_buf));
    // 接收usb数据，设置SN码
    while (1)
    {
        // 发送示例
        // const char *send_str = "Hello from ESP32-S3!\n";
        // usb_send_data(send_str, strlen(send_str));
        usb_receive_data();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
