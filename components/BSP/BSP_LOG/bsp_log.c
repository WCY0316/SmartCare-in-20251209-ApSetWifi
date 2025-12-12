#include "bsp_log.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sntp.h"
#include "time.h"
#include "esp_spiffs.h"
#include "tcpclient.h"
#include "wifi_ble_provisioning.h"
#include "tcpserver.h"
#include <dirent.h>
#include "camera.h"

#define TAG "LOGGER"
#define LOG_PARTITION_LABEL "spiffs"
#define MAX_LOG_FILES 4
#define LOG_FILE_SIZE 100 * 1024 // 每个文件100KB
#define LOG_CHECK_INTERVAL 100  // 每100条日志检查一次空间

static uint32_t log_entry_count = 0;
static int current_log_index = 1; // 当前活动日志文件索引

static bool isGetTime = false; // 是否获取到时间

// 设置时区为北京时间 (UTC+8)
void set_local_timezone(void)
{
    // 设置TZ环境变量：CST-8 表示中国标准时间，比UTC快8小时
    // 没有夏令时调整（DST=0）
    setenv("TZ", "CST-8", 1);
    tzset();
    ESP_LOGI(TAG, "Timezone set to Beijing Time (UTC+8)");
}
// 时间初始化函数
void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "cn.pool.ntp.org");
    sntp_setservername(1, "time.nist.gov");
    sntp_setservername(2, "ntp.aliyun.com");
    sntp_setservername(3, "210.72.145.44"); // 国家授时中心服务器 IP 地址
    sntp_init();
}

void VTimeTask(void *arg)
{
    set_local_timezone(); // 设置时区
    initialize_sntp();    // 初始化SNTP

    // 等待时间同步完成
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int retry_count = 20;
    isGetTime = false; // 初始化获取时间标志

    while (1)
    {
        if (IsWifiConnected() && IsServerConnected())
        {
            if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED)
            {
                time(&now);
                localtime_r(&now, &timeinfo);
                ESP_LOGI(TAG, "Time synchronized: %s", asctime(&timeinfo));
                // write_log("INFO", TAG, "Time synchronized: %s", asctime(&timeinfo));
                isGetTime = true; // 设置获取时间标志
                break;
            }
            else
            {
                ESP_LOGI(TAG, "Waiting for SNTP sync...");
                // write_log("INFO", TAG, "Waiting for SNTP sync ... %d", retry++);
                if (retry > 20)
                {
                    write_log("INFO", TAG, "SNTP sync failed after %d attempts", retry);
                    break;
                }
            }
        }
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

// 获取并打印当前时间
void print_current_time(char *buf, size_t buf_size)

{
    time_t now;
    struct tm timeinfo;
    time(&now);

    // 转换为本地时间
    localtime_r(&now, &timeinfo);

    if (isGetTime == false)
    {
        snprintf(buf, buf_size, "NULL");
        return;
    }

    // 检查时间是否有效
    if (timeinfo.tm_year < (2023 - 1900))
    {
        ESP_LOGE(TAG, "Time is not set yet. Set time using SNTP first.");
        snprintf(buf, buf_size, "NULL");
        return;
    }

    strftime(buf, buf_size, "%Y-%m-%d %H:%M:%S", &timeinfo); // 格式化时间字符串
    // ESP_LOGI(TAG, "Current time: %s", buf);

    // 分别获取年月日时分秒
    int year = timeinfo.tm_year + 1900; // tm_year是从1900开始的偏移量
    int month = timeinfo.tm_mon + 1;    // tm_mon范围是0-11
    int day = timeinfo.tm_mday;         // 月份中的日期，范围1-31
    int hour = timeinfo.tm_hour;        // 小时，范围0-23
    int minute = timeinfo.tm_min;       // 分钟，范围0-59
    int second = timeinfo.tm_sec;       // 秒，范围0-60（可能包含闰秒）

    // ESP_LOGI(TAG, "Year: %d, Month: %d, Day: %d, Hour: %d, Minute: %d, Second: %d",
    //          year, month, day, hour, minute, second);
    // 返回当前时间字符串
}

// 初始化SPIFFS分区
esp_err_t init_log_storage(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = LOG_PARTITION_LABEL,
        .max_files = MAX_LOG_FILES + 2,
        .format_if_mount_failed = true};

    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(err));
        return err;
    }

    // 查找可用的日志文件
    current_log_index = find_available_log_file();

    // 如果没有找到任何日志文件，创建log1.txt
    if (current_log_index == 1)
    {
        char path[32];
        get_log_path(1, path, sizeof(path));

        struct stat st;
        if (stat(path, &st) != 0)
        {
            // 文件不存在，创建它
            FILE *fp = fopen(path, "w");
            if (fp != NULL)
            {
                fclose(fp);
                ESP_LOGI(TAG, "Created initial log file: log1.txt");
            }
            else
            {
                ESP_LOGE(TAG, "Failed to create initial log file");
            }
        }
    }

    ESP_LOGI(TAG, "Starting with log file: log%d.txt", current_log_index);
    // write_log("INFO", TAG, "\n\nStarting with log file: log%d.txt", current_log_index);

    size_t total, used;
    err = esp_spiffs_info(LOG_PARTITION_LABEL, &total, &used);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "SPIFFS: Total=%dKB, Used=%dKB", total / 1024, used / 1024);
        write_log("INFO", TAG, "SPIFFS: Total=%dKB, Used=%dKB", total / 1024, used / 1024);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to get SPIFFS info (%s)", esp_err_to_name(err));
    }

    return ESP_OK;
}
// 获取指定序号的日志文件路径
static void get_log_path(int index, char *path, size_t size)
{
    snprintf(path, size, "/spiffs/log%d.txt", index);
}

// 查找可用的日志文件（程序启动时调用）
static int find_available_log_file(void)
{
    for (int i = 1; i <= MAX_LOG_FILES; i++)
    {
        char path[32];
        get_log_path(i, path, sizeof(path));

        struct stat st;
        if (stat(path, &st) != 0)
        {
            // 文件不存在，返回当前索引
            return i;
        }

        // 文件存在，检查是否已满
        if (st.st_size < LOG_FILE_SIZE)
        {
            return i;
        }
    }

    // 所有文件都满了，返回1（将被清空）
    return 1;
}

// 确保日志文件存在，如果不存在则创建
static void ensure_log_file_exists(int index)
{
    char path[32];
    get_log_path(index, path, sizeof(path));

    struct stat st;
    if (stat(path, &st) != 0)
    {
        // 文件不存在，创建它
        FILE *fp = fopen(path, "w");
        if (fp != NULL)
        {
            fclose(fp);
            ESP_LOGI(TAG, "Created log file: %s", path);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to create log file: %s", path);
        }
    }
}
// 检查并切换到下一个日志文件
static void check_and_rotate_log(void)
{
    char path[32];
    get_log_path(current_log_index, path, sizeof(path));

    struct stat st;
    if (stat(path, &st) == 0 && st.st_size >= LOG_FILE_SIZE)
    {
        // 当前文件已满，切换到下一个
        current_log_index = (current_log_index % MAX_LOG_FILES) + 1;
        ESP_LOGI(TAG, "Switched to log file: log%d.txt", current_log_index);

        // 确保新的日志文件存在
        ensure_log_file_exists(current_log_index);
        get_log_path(current_log_index, path, sizeof(path));
        // 检查新文件是否存在且已满（理论上不会，但保险起见）
        if (stat(path, &st) == 0 && st.st_size >= LOG_FILE_SIZE)
        {
            FILE *fp = fopen(path, "w"); // 清空文件
            if (fp != NULL)
            {
                fclose(fp);
                ESP_LOGI(TAG, "Cleared full log file: log%d.txt", current_log_index);
            }
        }
    }
}

// 检查并清理空间
static void check_storage_space(void)
{
    if (++log_entry_count % LOG_CHECK_INTERVAL != 0)
    {
        return;
    }

    size_t total, used;
    if (esp_spiffs_info(LOG_PARTITION_LABEL, &total, &used) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to check storage space");
        return;
    }

    if ((used * 100 / total) > 95)
    {
        ESP_LOGW(TAG, "Storage space low: %d%% used", (used * 100 / total));
        // 强制切换到下一个文件
        current_log_index = (current_log_index % MAX_LOG_FILES) + 1;
        ESP_LOGI(TAG, "Forced switch to log file: log%d.txt", current_log_index);

        // 确保新文件存在
        ensure_log_file_exists(current_log_index);
    }
}

// 写入日志条目（线程安全）
void write_log(const char *level, const char *tag, const char *format, ...)
{
    static SemaphoreHandle_t log_mutex = NULL;
    if (log_mutex == NULL)
    {
        log_mutex = xSemaphoreCreateMutex();
    }

    xSemaphoreTake(log_mutex, portMAX_DELAY);

    // 检查是否需要切换日志文件
    check_and_rotate_log();

    char log_path[64];
    get_log_path(current_log_index, log_path, sizeof(log_path));

    // 打开文件追加日志
    FILE *fp = fopen(log_path, "a");
    if (fp == NULL)
    {
        ESP_LOGE(TAG, "Failed to open log file: %s", log_path);
        xSemaphoreGive(log_mutex);
        return;
    }

    // 获取当前时间戳
    char time_buf[32];
    print_current_time(time_buf, sizeof(time_buf));
    // printf("Current time: %s\n", time_buf);

    // 格式化日志内容
    char log_buf[512];
    va_list args;
    va_start(args, format);
    vsnprintf(log_buf, sizeof(log_buf), format, args);
    va_end(args);

    // 写入日志条目
    fprintf(fp, "[%s] [%s] [%s] %s\r\n", time_buf, level, tag, log_buf);
    fflush(fp);
    fclose(fp);

    check_storage_space();
    xSemaphoreGive(log_mutex);
}
// 读取指定日志文件并通过串口输出
esp_err_t print_log_file(int index)
{
    char buffer[1024];
    char path[32];
    get_log_path(index, path, sizeof(path));

    FILE *fp = fopen(path, "r");
    if (fp == NULL)
    {
        ESP_LOGE(TAG, "Log file %d not found", index);
        return ESP_ERR_NOT_FOUND;
    }

    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        printf("%s", buffer);
    }

    fclose(fp);
    return ESP_OK;
}
// 读取指定日志文件并通过tcp发送
esp_err_t print_log_file_tcp(int index)
{
    char buffer[1024];
    char path[32];
    get_log_path(index, path, sizeof(path));

    FILE *fp = fopen(path, "r");
    if (fp == NULL)
    {
        ESP_LOGE(TAG, "Log file %d not found", index);
        return ESP_ERR_NOT_FOUND;
    }

    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        TcpSendServer((uint8_t *)buffer, strlen(buffer));
    }

    fclose(fp);
    return ESP_OK;
}

/**
 * 清空所有日志文件（log1.txt~log10.txt）
 * @return ESP_OK 成功 | 其他错误码 失败
 */
esp_err_t clear_all_log_files(void)
{
    // 遍历所有日志文件
    for (int i = 1; i <= MAX_LOG_FILES; i++)
    {
        char path[32];
        get_log_path(i, path, sizeof(path));

        // 打开文件以写入模式（会自动清空文件内容）
        FILE *fp = fopen(path, "w");
        if (fp == NULL)
        {
            // 文件不存在或无法打开，尝试删除
            if (remove(path) != 0)
            {
                ESP_LOGE(TAG, "Failed to clear or remove log%d.txt", i);
                // 继续处理其他文件，不中断整个操作
            }
            continue;
        }

        // 关闭文件（已清空）
        fclose(fp);
        ESP_LOGI(TAG, "Cleared log file: %s", path);
    }

    // 重置当前日志索引为1
    current_log_index = 1;

    return ESP_OK;
}
/**
 * 查询所有日志文件的大小并打印
 * @return ESP_OK 成功 | 其他错误码 失败
 */
esp_err_t print_all_log_file_sizes(void)
{
    ESP_LOGI(TAG, "=== Log File Sizes ===");

    size_t total_size = 0;
    bool all_files_exist = true;

    // 遍历所有日志文件
    for (int i = 1; i <= MAX_LOG_FILES; i++)
    {
        char path[32];
        get_log_path(i, path, sizeof(path));

        struct stat st;
        if (stat(path, &st) == 0)
        {
            // 文件存在，打印大小
            size_t file_size = st.st_size;
            total_size += file_size;

            ESP_LOGI(TAG, "log%d.txt: %zu bytes (%zu KB)",
                     i, file_size, file_size / 1024);
        }
        else
        {
            // 文件不存在
            ESP_LOGI(TAG, "log%d.txt: Not found", i);
            all_files_exist = false;
        }
    }

    // 打印总大小
    ESP_LOGI(TAG, "Total size: %zu bytes (%zu KB)",
             total_size, total_size / 1024);

    // 打印SPIFFS分区使用情况
    size_t total, used;
    if (esp_spiffs_info(LOG_PARTITION_LABEL, &total, &used) == ESP_OK)
    {
        ESP_LOGI(TAG, "SPIFFS Total: %zu KB, Used: %zu KB, Free: %zu KB",
                 total / 1024, used / 1024, (total - used) / 1024);
    }

    return all_files_exist ? ESP_OK : ESP_ERR_NOT_FOUND;
}
/**
 * 通过TCP发送日志文件大小信息
 * @param sock 已连接的TCP socket描述符
 * @return ESP_OK 成功 | 其他错误码 失败
 */
esp_err_t send_log_file_sizes(void)
{
    char buffer[1024];
    int bytes_sent = 0;
    size_t total_size = 0;

    // 发送标题行
    const char *header = "=== Log File Sizes ===\r\n";
    if (TcpSendServer((uint8_t *)header, strlen(header)) < 0)
    {
        ESP_LOGE(TAG, "Failed to send header");
        return ESP_FAIL;
    }
    bytes_sent += strlen(header);

    // 遍历所有日志文件并发送大小信息
    for (int i = 1; i <= MAX_LOG_FILES; i++)
    {
        char path[32];
        get_log_path(i, path, sizeof(path));

        struct stat st;
        if (stat(path, &st) == 0)
        {
            // 文件存在，发送大小信息
            size_t file_size = st.st_size;
            total_size += file_size;

            int len = snprintf(buffer, sizeof(buffer),
                               "log%d.txt: %zu bytes (%zu KB)\r\n",
                               i, file_size, file_size / 1024);

            if (TcpSendServer((uint8_t *)buffer, len) < 0)
            {
                ESP_LOGE(TAG, "Failed to send log%d.txt size", i);
                return ESP_FAIL;
            }
            bytes_sent += len;
        }
        else
        {
            // 文件不存在
            int len = snprintf(buffer, sizeof(buffer),
                               "log%d.txt: Not found\r\n", i);

            if (TcpSendServer((uint8_t *)buffer, len) < 0)
            {
                ESP_LOGE(TAG, "Failed to send log%d.txt status", i);
                return ESP_FAIL;
            }
            bytes_sent += len;
        }
    }

    // 发送总大小信息
    int len = snprintf(buffer, sizeof(buffer),
                       "Total size: %zu bytes (%zu KB)\r\n",
                       total_size, total_size / 1024);

    if (TcpSendServer((uint8_t *)buffer, len) < 0)
    {
        ESP_LOGE(TAG, "Failed to send total size");
        return ESP_FAIL;
    }
    bytes_sent += len;

    // 发送SPIFFS分区使用情况
    size_t spiffs_total, spiffs_used;
    if (esp_spiffs_info(LOG_PARTITION_LABEL, &spiffs_total, &spiffs_used) == ESP_OK)
    {
        len = snprintf(buffer, sizeof(buffer),
                       "SPIFFS Total: %zu KB, Used: %zu KB, Free: %zu KB\r\n",
                       spiffs_total / 1024, spiffs_used / 1024, (spiffs_total - spiffs_used) / 1024);

        if (TcpSendServer((uint8_t *)buffer, len) < 0)
        {
            ESP_LOGE(TAG, "Failed to send SPIFFS info");
            return ESP_FAIL;
        }
        bytes_sent += len;
    }

    ESP_LOGI(TAG, "Successfully sent %d bytes of log file sizes", bytes_sent);
    return ESP_OK;
}


esp_err_t send_picbuf(void)
{
    char buffer[1024];
    int bytes_sent = 0;
    size_t total_size = 0;

    // 发送标题行
    const char *header = "=== Pic Buf ===\r\n";
    if (TcpSendServer((uint8_t *)header, strlen(header)) < 0)
    {
        ESP_LOGE(TAG, "Failed to send header");
        return ESP_FAIL;
    }
    flashPictureSize = 614400;
    bytes_sent += strlen(header);
    uint8_t *buf = (uint8_t *)malloc(flashPictureSize + 4);

    ReadPicFromFlash(buf, flashPictureSize);

    

    if (TcpSendServer((uint8_t *)buf, flashPictureSize) < 0)
    {
        ESP_LOGE(TAG, "Failed to send total size");
        return ESP_FAIL;
    }
    // bytes_sent += len;
    free(buf);

    ESP_LOGI(TAG, "Successfully sent %d bytes of Pic", flashPictureSize);
    return ESP_OK;
}