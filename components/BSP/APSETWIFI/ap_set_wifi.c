#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include <esp_wifi.h>
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_http_server.h"
#include "ap_set_wifi.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "lwip/inet.h"
#include "lwip/ip4_addr.h"
#include "freertos/timers.h"

static const char *TAG = "AP_CONFIG";

#define WIFI_CONNECT_TIMEOUT_MS 1000 * 1000 * 15 // WiFi连接超时时间ms

/* AP配置 */
// #define AP_SSID "ESP32-S3_Config"
uint8_t AP_SSID[32] = "ESP32-Config";
#define AP_PASSWORD "12345678"
#define AP_CHANNEL 1
#define AP_MAX_CONN 4

/* 事件组标志位 */
#define WIFI_CONNECTED_BIT BIT0       // 连接成功
#define WIFI_FAIL_BIT BIT1            // 连接失败
#define WIFI_CONNECT_TIMEOUT_BIT BIT3 // 连接超时
#define WIFI_AP_STOP_BIT BIT2         // AP模式停止

/* 函数声明 */
static void url_decode(char *dst, const char *src);
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
static void wifi_init_softap(void);
static esp_err_t root_get_handler(httpd_req_t *req);
static esp_err_t configure_post_handler(httpd_req_t *req);
static esp_err_t status_get_handler(httpd_req_t *req);
static esp_err_t favicon_get_handler(httpd_req_t *req);
static void start_webserver(void);
static void stop_webserver(void);
static void stop_ap_mode(void);
static void wifi_connect_task(void *pvParameters);

static EventGroupHandle_t wifi_event_group;
static httpd_handle_t server = NULL;
static char wifi_ssid[32] = {0};
static char wifi_password[64] = {0};
static bool is_connected = false;
static bool ap_active = true;

// TimerHandle_t wifi_connect_timer; // 连接超时定时器句柄

// WiFi扫描结果结构体
typedef struct
{
    char ssid[33];   // WiFi名称（最大32字符+1）
    int8_t rssi;     // 信号强度（dBm）
    uint8_t channel; // 信道
} wifi_scan_result_t;

// // HTML配置页面
// static const char *CONFIG_PAGE =
//     "<!DOCTYPE html>"
//     "<html>"
//     "<head>"
//     "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
//     "<style>"
//     "body { font-family: Arial; margin: 40px; }"
//     ".container { max-width: 400px; margin: 0 auto; }"
//     "input { width: 100%; padding: 12px; margin: 8px 0; border: 1px solid #ccc; }"
//     "button { background-color: #4CAF50; color: white; padding: 14px; border: none; cursor: pointer; width: 100%; }"
//     ".status { padding: 10px; margin: 10px 0; border-radius: 5px; }"
//     ".success { background-color: #d4edda; color: #155724; }"
//     ".error { background-color: #f8d7da; color: #721c24; }"
//     ".info { background-color: #d1ecf1; color: #0c5460; }"
//     "</style>"
//     "</head>"
//     "<body>"
//     "<div class=\"container\">"
//     "<h2>ESP32-S3 Network Configuration</h2>"
//     "<div id=\"status\" class=\"status info\">Connected to AP: " AP_SSID "</div>"
//     "<form id=\"configForm\">"
//     "<label>WiFi SSID:</label>"
//     "<input type=\"text\" id=\"ssid\" name=\"ssid\" placeholder=\"Enter WiFi SSID\" required>"
//     "<label>WiFi Password:</label>"
//     "<input type=\"password\" id=\"password\" name=\"password\" placeholder=\"Enter WiFi password\">"
//     "<button type=\"submit\">Connect to Network</button>"
//     "</form>"
//     "<script>"
//     "let refreshInterval;"
//     "document.getElementById('configForm').onsubmit = async function(e) {"
//     "e.preventDefault();"
//     "const ssid = document.getElementById('ssid').value;"
//     "const password = document.getElementById('password').value;"
//     "const statusDiv = document.getElementById('status');"
//     "statusDiv.className = 'status info';"
//     "statusDiv.innerHTML = 'Connecting to network...';"
//     "clearInterval(refreshInterval);"
//     "refreshInterval = setInterval(checkConnection, 3000);"
//     "try {"
//     "const response = await fetch('/configure', {"
//     "method: 'POST',"
//     "headers: {'Content-Type': 'application/x-www-form-urlencoded'},"
//     "body: 'ssid=' + encodeURIComponent(ssid) + '&password=' + encodeURIComponent(password)"
//     "});"
//     "const data = await response.json();"
//     "if(data.status === 'connecting') {"
//     "statusDiv.className = 'status info';"
//     "statusDiv.innerHTML = 'Connecting to network, please wait...';"
//     "checkConnection();"
//     "} else {"
//     "statusDiv.className = 'status error';"
//     "statusDiv.innerHTML = 'Configuration failed: ' + data.message;"
//     "clearInterval(refreshInterval);"
//     "}"
//     "} catch(error) {"
//     "statusDiv.className = 'status error';"
//     "statusDiv.innerHTML = 'Network error: ' + error;"
//     "clearInterval(refreshInterval);"
//     "}"
//     "};"
//     "async function checkConnection() {"
//     "const statusDiv = document.getElementById('status');"
//     "try {"
//     "const response = await fetch('/status');"
//     "const data = await response.json();"
//     "if(data.connected) {"
//     "statusDiv.className = 'status success';"
//     "statusDiv.innerHTML = 'Connection successful! ' + '<br>Network verification completed.';"
//     "setTimeout(() => { statusDiv.innerHTML += '<br>AP will be closed shortly...'; }, 2000);"
//     "clearInterval(refreshInterval);"
//     "} else {"
//     "statusDiv.className = 'status error';"
//     "statusDiv.innerHTML = 'Connection failed: ' + data.message;"
//     "}"
//     "} catch(error) {"
//     "}"
//     "}"
//     "</script>"
//     "</div>"
//     "</body>"
//     "</html>";

static const char *CONFIG_PAGE1 =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<meta charset=\"UTF-8\">" // 添加UTF-8编码声明
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<style>"
    "body { font-family: Arial; margin: 40px; }"
    ".container { max-width: 400px; margin: 0 auto; }"
    "input, select { width: 100%; padding: 12px; margin: 8px 0; border: 1px solid #ccc; box-sizing: border-box; }"
    "button { background-color: #4CAF50; color: white; padding: 14px; border: none; cursor: pointer; width: 100%; }"
    ".status { padding: 10px; margin: 10px 0; border-radius: 5px; }"
    ".success { background-color: #d4edda; color: #155724; }"
    ".error { background-color: #f8d7da; color: #721c24; }"
    ".info { background-color: #d1ecf1; color: #0c5460; }"
    "</style>"
    "</head>"
    "<body>"
    "<div class=\"container\">"
    "<h2>";

static const char *CONFIG_PAGE2 =
    " 网络配置</h2>"
    "<div id=\"status\" class=\"status info\">请输入WiFi信息</div>"
    "<form id=\"configForm\">"
    "<label>WiFi 名称:</label>"
    "<select id=\"ssid\" name=\"ssid\" required>"
    "<option value=\"\" disabled selected>选择 WiFi</option>"
    "</select>"
    "<label>WiFi 密码:</label>"
    "<input type=\"password\" id=\"password\" name=\"password\" placeholder=\" 输入 WiFi 密码\">"
    "<button type=\"submit\">连接</button>"
    "</form>"
    "<script>"
    "let refreshInterval;"
    "window.onload = loadWifiList;"
    "function getSignalText(rssi) {"
    "  if (rssi >= -50) return '信号极强';"
    "  if (rssi >= -60) return '信号强';"
    "  if (rssi >= -70) return '信号良好';"
    "  if (rssi >= -80) return '信号一般';"
    "  if (rssi >= -90) return '信号弱';"
    "  return '信号极差';"
    "}"
    "async function loadWifiList() {"
    "  const statusDiv = document.getElementById('status');"
    "  try {"
    "    const response = await fetch('/wifi-list');"
    "    if (!response.ok) throw new Error('Failed to fetch WiFi list');"
    "    const wifiList = await response.json();"
    "    const ssidSelect = document.getElementById('ssid');"
    "    ssidSelect.innerHTML = '<option value=\"\" disabled selected>选择 WiFi</option>';"
    "    wifiList.forEach(wifi => {"
    "      const option = document.createElement('option');"
    "      option.value = wifi.ssid;"
    // "      option.textContent = `${wifi.ssid} (${getSignalText(wifi.rssi)})`;"
    "      option.textContent = `${wifi.ssid}`;"
    "      ssidSelect.appendChild(option);"
    "    });"
    "  } catch (error) {"
    "    statusDiv.className = 'status error';"
    "    statusDiv.innerHTML = 'Load WiFi list failed: ' + error.message;"
    "  }"
    "}"
    "document.getElementById('configForm').onsubmit = async function(e) {"
    "e.preventDefault();"
    "const ssid = document.getElementById('ssid').value;"
    "const password = document.getElementById('password').value;"
    "const statusDiv = document.getElementById('status');"
    "statusDiv.className = 'status info';"
    "statusDiv.innerHTML = '正在连接...';"
    "clearInterval(refreshInterval);"
    "refreshInterval = setInterval(checkConnection, 3000);"
    "try {"
    "const response = await fetch('/configure', {"
    "method: 'POST',"
    "headers: {'Content-Type': 'application/x-www-form-urlencoded'},"
    "body: 'ssid=' + encodeURIComponent(ssid) + '&password=' + encodeURIComponent(password)"
    "});"
    "const data = await response.json();"
    "if(data.status === 'connecting') {"
    "statusDiv.className = 'status info';"
    "statusDiv.innerHTML = '正在连接，请等待...';"
    "checkConnection();"
    "} else {"
    "statusDiv.className = 'status error';"
    "statusDiv.innerHTML = '配置失败: ' + data.message;"
    "clearInterval(refreshInterval);"
    "}"
    "} catch(error) {"
    "statusDiv.className = 'status error';"
    "statusDiv.innerHTML = '网络错误: ' + error;"
    "clearInterval(refreshInterval);"
    "}"
    "};"
    "async function checkConnection() {"
    "const statusDiv = document.getElementById('status');"
    "try {"
    "const response = await fetch('/status');"
    "const data = await response.json();"
    "if(data.connected) {"
    "statusDiv.className = 'status success';"
    "statusDiv.innerHTML = '连接成功！ ' + '<br>网络验证完成.';"
    "setTimeout(() => { statusDiv.innerHTML += '<br>设备热点即将关闭...'; }, 2000);"
    "clearInterval(refreshInterval);"
    "} else {"
    "statusDiv.className = 'status error';"
    "statusDiv.innerHTML = '' + data.message;"
    "}"
    "} catch(error) {"
    "}"
    "}"
    "</script>"
    "</div>"
    "</body>"
    "</html>";
static esp_err_t http_handler_scan(httpd_req_t *req);

/* URL解码函数 */
static void url_decode(char *dst, const char *src)
{
    char a, b;
    while (*src)
    {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) && (isxdigit(a) && isxdigit(b)))
        {
            if (a >= 'a')
                a -= 'a' - 'A';
            if (a >= 'A')
                a -= ('A' - 10);
            else
                a -= '0';
            if (b >= 'a')
                b -= 'a' - 'A';
            if (b >= 'A')
                b -= ('A' - 10);
            else
                b -= '0';
            *dst++ = 16 * a + b;
            src += 3;
        }
        else if (*src == '+')
        {
            *dst++ = ' ';
            src++;
        }
        else
        {
            *dst++ = *src++;
        }
    }
    *dst++ = '\0';
}

/* 添加favicon.ico处理函数 */
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    // 返回一个空的favicon响应，避免404错误
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static TimerHandle_t s_wifi_timer = NULL;
/**
 * WiFi连接超时回调：超时后置位失败位，标记连接失败
 */
static void wifi_connect_timeout_cb(TimerHandle_t timer)
{
    ESP_LOGW(TAG, "WiFi connect timeout (%dms)", WIFI_CONNECT_TIMEOUT_MS);
    is_connected = false;
    esp_err_t err = esp_wifi_disconnect();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to disconnect WiFi (err: %d)", err);
        // return err;
    }
    ESP_LOGI(TAG, "Forced to disconnect WiFi (connection process interrupted)");

    xEventGroupSetBits(wifi_event_group, WIFI_CONNECT_TIMEOUT_BIT);
    // 删除定时器（避免重复触发）
    esp_timer_stop(s_wifi_timer);   // 停止定时器
    esp_timer_delete(s_wifi_timer); // 删除定时器
    s_wifi_timer = NULL;
}

/* WiFi事件处理 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base != WIFI_EVENT)
    {
        // 非WiFi事件直接返回（原代码注释的IP事件可移到此处处理）
        if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
        {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        }
        return;
    }

    switch (event_id)
    {
    case WIFI_EVENT_AP_START:
        ESP_LOGI(TAG, "AP mode started successfully");
        break;

    case WIFI_EVENT_STA_START:
        ESP_LOGI(TAG, "STA mode started");

        break;

    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG, "Connected to AP: %s", wifi_ssid);
        // ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        // ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        is_connected = true;

        // 连接成功：停止并删除超时定时器（避免触发超时）
        if (s_wifi_timer != NULL)
        {
            esp_timer_stop(s_wifi_timer);   // 停止定时器
            esp_timer_delete(s_wifi_timer); // 删除定时器
            s_wifi_timer = NULL;
        }
        break;

    case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "WiFi disconnected");
        // 断开连接：停止定时器（避免重复超时）
        if (s_wifi_timer != NULL)
        {
            esp_timer_stop(s_wifi_timer);   // 停止定时器
            esp_timer_delete(s_wifi_timer); // 删除定时器
            s_wifi_timer = NULL;
        }
        if (!is_connected)
        {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        break;

    case WIFI_EVENT_AP_STOP:
        ESP_LOGI(TAG, "AP mode stopped");
        xEventGroupSetBits(wifi_event_group, WIFI_AP_STOP_BIT);
        break;
    }
}

/* 初始化AP模式 */
static void wifi_init_softap(void)
{
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "ESP32-Config",
            .password = AP_PASSWORD,
            .ssid_len = strlen((const char *)AP_SSID),
            .channel = AP_CHANNEL,
            .max_connection = AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };
    // 将deviceSN作为AP名称
    memcpy(wifi_config.ap.ssid, deviceSN, strlen(deviceSN));
    wifi_config.ap.ssid_len = strlen(deviceSN);

    if (strlen(AP_PASSWORD) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "AP mode started");
    ESP_LOGI(TAG, "SSID: %s", AP_SSID);
    ESP_LOGI(TAG, "Password: %s", AP_PASSWORD);
    ESP_LOGI(TAG, "IP address: 192.168.4.1");
}

/* HTTP请求处理函数 */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    // 计算总长度
    size_t total_len = strlen(CONFIG_PAGE1) + strlen(deviceSN) + strlen(CONFIG_PAGE2) + 1;

    // 分配内存
    char *html = (char *)malloc(total_len);
    if (html == NULL)
    {
        return NULL;
    }

    // 拼接字符串
    strcpy(html, CONFIG_PAGE1);
    strcat(html, deviceSN);
    strcat(html, CONFIG_PAGE2);

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, html, strlen(html));

    free(html);
    return ESP_OK;
}

static esp_err_t configure_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;

    if (remaining > sizeof(buf) - 1)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Content too long");
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
        return ESP_FAIL;
    }

    buf[ret] = '\0';

    // 解析表单数据
    char *ssid_start = strstr(buf, "ssid=");
    char *password_start = strstr(buf, "password=");

    if (ssid_start)
    {
        ssid_start += 5;
        char *ssid_end = strchr(ssid_start, '&');
        if (ssid_end)
        {
            *ssid_end = '\0';
        }
        url_decode(wifi_ssid, ssid_start);
    }

    if (password_start)
    {
        password_start += 9;
        url_decode(wifi_password, password_start);
    }

    ESP_LOGI(TAG, "Received configuration: SSID=%s, Password=%s", wifi_ssid, wifi_password);

    // 重置连接状态
    is_connected = false;
    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    // 尝试连接WiFi
    wifi_config_t wifi_sta_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    strncpy((char *)wifi_sta_config.sta.ssid, wifi_ssid, sizeof(wifi_sta_config.sta.ssid));
    strncpy((char *)wifi_sta_config.sta.password, wifi_password, sizeof(wifi_sta_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));
    ESP_ERROR_CHECK(esp_wifi_connect());

    char response[200];
    snprintf(response, sizeof(response),
             "{\"status\":\"connecting\",\"message\":\"Connecting to %s, please wait...\"}", wifi_ssid);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);

    // 清除超时标志位，准备下一次连接
    xEventGroupClearBits(wifi_event_group, WIFI_CONNECT_TIMEOUT_BIT);

    // 启动定时器（若已存在则重置）
    if (s_wifi_timer != NULL)
    {
        xTimerStop(s_wifi_timer, 0);
        xTimerDelete(s_wifi_timer, 0);
        s_wifi_timer = NULL;
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
    // 创建并启动超时定时器
    if (s_wifi_timer == NULL)
    {
        const esp_timer_create_args_t s_wifi_timer = {
            .callback = &wifi_connect_timeout_cb,
            /* name is optional, but may help identify the timer when debugging */
            .name = "periodic"};
        esp_timer_handle_t wifi_timer;
        ESP_ERROR_CHECK(esp_timer_create(&s_wifi_timer, &wifi_timer));
        ESP_ERROR_CHECK(esp_timer_start_once(wifi_timer, WIFI_CONNECT_TIMEOUT_MS)); // 微秒
    }

    return ESP_OK;
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    char response[200];

    // 检查是否已连接到WiFi
    wifi_ap_record_t ap_info;
    esp_err_t wifi_status = esp_wifi_sta_get_ap_info(&ap_info);

    if (wifi_status == ESP_OK && strlen((char *)ap_info.ssid) > 0)
    {
        // 已连接到WiFi，获取IP信息
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);

        snprintf(response, sizeof(response),
                 "{\"connected\":true,\"ip\":\"" IPSTR "\",\"ssid\":\"%s\",\"message\":\"连接成功\"}",
                 IP2STR(&ip_info.ip), (char *)ap_info.ssid);
        is_connected = true;
    }
    else if (is_connected)
    {
        // 如果is_connected标志为true但无法获取AP信息，尝试获取IP信息
        esp_netif_ip_info_t ip_info;
        esp_err_t err = esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);
        if (err == ESP_OK)
        {
            snprintf(response, sizeof(response),
                     "{\"connected\":true,\"ip\":\"" IPSTR "\",\"ssid\":\"%s\",\"message\":\"连接成功\"}",
                     IP2STR(&ip_info.ip), wifi_ssid);
        }
        else
        {
            snprintf(response, sizeof(response),
                     "{\"connected\":false,\"message\":\"等待IP地址...\"}");
        }
    }
    else
    {
        // 超时
        if (xEventGroupGetBits(wifi_event_group) & WIFI_CONNECT_TIMEOUT_BIT)
        {
            snprintf(response, sizeof(response),
                     "{\"connected\":false,\"message\":\"连接超时，请重试.\"}");
            // 清除超时标志位，准备下一次连接
            // xEventGroupClearBits(wifi_event_group, WIFI_CONNECT_TIMEOUT_BIT);
        }
        else
        { // 未连接
            // snprintf(response, sizeof(response),
            //          "{\"connected\":false,\"message\":\"正在连接 WiFi...\"}");
        }
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);

    // ESP_LOGI(TAG, "Status response: %s", response);

    return ESP_OK;
}

/* 启动HTTP服务器 */
static void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.ctrl_port = 32768;

    ESP_LOGI(TAG, "Starting web server on port: %d", config.server_port);

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_get_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &root_uri);

        httpd_uri_t configure_uri = {
            .uri = "/configure",
            .method = HTTP_POST,
            .handler = configure_post_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &configure_uri);

        httpd_uri_t status_uri = {
            .uri = "/status",
            .method = HTTP_GET,
            .handler = status_get_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &status_uri);

        /* 添加favicon.ico处理，避免404错误 */
        httpd_uri_t favicon_uri = {
            .uri = "/favicon.ico",
            .method = HTTP_GET,
            .handler = favicon_get_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &favicon_uri);

        // 添加WiFi扫描端点
        httpd_uri_t scan_uri = {
            .uri = "/wifi-list",
            .method = HTTP_GET,
            .handler = http_handler_scan,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &scan_uri);
    }
}

/* 停止HTTP服务器 */
static void stop_webserver(void)
{
    if (server)
    {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "Web server stopped");
    }
}

/* 关闭AP模式 */
static void stop_ap_mode(void)
{
    ESP_LOGI(TAG, "Stopping AP mode...");

    // 先停止HTTP服务器
    stop_webserver();

    // 延迟确保所有HTTP连接关闭
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // 停止AP模式
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ap_active = false;

    ESP_LOGI(TAG, "AP mode stopped, device now running in STA mode only");
}

/* WiFi连接任务 */
static void wifi_connect_task(void *pvParameters)
{
    EventBits_t bits;

    while (1)
    {
        bits = xEventGroupWaitBits(wifi_event_group,
                                   WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                   pdFALSE, pdFALSE, 60000 / portTICK_PERIOD_MS); // 60秒超时

        if (bits & WIFI_CONNECTED_BIT)
        {
            ESP_LOGI(TAG, "WiFi connected successfully");

            // 添加额外延迟，确保网络稳定
            vTaskDelay(2000 / portTICK_PERIOD_MS);

            // 验证连接状态
            wifi_ap_record_t ap_info;
            if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
            {
                ESP_LOGI(TAG, "Verified connection to: %s, RSSI: %d", ap_info.ssid, ap_info.rssi);
                is_connected = true;
            }
            else
            {
                ESP_LOGW(TAG, "Failed to verify connection, retrying...");
                xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
                continue;
            }

            // 延迟显示成功信息
            vTaskDelay(3000 / portTICK_PERIOD_MS);

            // 关闭AP模式
            if (ap_active)
            {
                stop_ap_mode();
            }

            // 保存WiFi配置到NVS
            nvs_handle_t nvs_handle;
            esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
            if (err == ESP_OK)
            {
                nvs_set_str(nvs_handle, "wifi_ssid", wifi_ssid);
                nvs_set_str(nvs_handle, "wifi_pass", wifi_password);
                nvs_commit(nvs_handle);
                nvs_close(nvs_handle);
                ESP_LOGI(TAG, "WiFi configuration saved to NVS");
            }

            vTaskDelete(NULL);
        }

        if (bits & WIFI_FAIL_BIT)
        {
            ESP_LOGI(TAG, "WiFi connection failed");
            is_connected = false;
            xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
        }

        // 超时处理
        if ((bits & (WIFI_CONNECTED_BIT | WIFI_FAIL_BIT)) == 0)
        {
            ESP_LOGW(TAG, "Connection timeout. Please try again.");
            is_connected = false;
            xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
        }
    }
}

// 判断是否为2.4GHz频段
static bool is_2_4ghz_channel(uint8_t channel)
{
    return (channel >= 1 && channel <= 13);
}

// WiFi扫描回调函数
static void wifi_scan_done_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_SCAN_DONE)
    {
        TaskHandle_t task_to_notify = (TaskHandle_t)arg;
        xTaskNotifyGive(task_to_notify);
    }
}

// 执行WiFi扫描并返回JSON格式字符串
char *wifi_scan_to_json(void)
{
    ESP_LOGI(TAG, "Starting WiFi scan...");

    // 创建任务通知句柄
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();

    // 注册扫描完成事件
    esp_event_handler_instance_t instance_scan_done;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        WIFI_EVENT_SCAN_DONE,
                                                        &wifi_scan_done_handler,
                                                        current_task,
                                                        &instance_scan_done));

    // 配置扫描参数
    wifi_scan_config_t scan_config = {
        .ssid = NULL,         // 扫描所有SSID
        .bssid = NULL,        // 扫描所有BSSID
        .channel = 0,         // 扫描所有信道
        .show_hidden = false, // 不显示隐藏网络
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = {
                .min = 100,
                .max = 300}}};

    // 开始扫描
    esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start WiFi scan: %s", esp_err_to_name(ret));
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, instance_scan_done);
        return NULL;
    }

    // 等待扫描完成（最多10秒）
    ESP_LOGI(TAG, "Waiting for scan to complete...");
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10000)) == 0)
    {
        ESP_LOGE(TAG, "WiFi scan timeout");
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, instance_scan_done);
        return NULL;
    }

    // 获取扫描结果数量
    uint16_t ap_count = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Found %d access points", ap_count);

    if (ap_count == 0)
    {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, instance_scan_done);
        char *empty_json = malloc(3); // "[]"
        if (empty_json)
        {
            strcpy(empty_json, "[]");
        }
        return empty_json;
    }

    // 分配内存获取扫描结果
    wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (ap_records == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for scan results");
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, instance_scan_done);
        return NULL;
    }

    // 获取扫描结果
    uint16_t ap_count_actual = ap_count;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count_actual, ap_records));

    // 只统计2.4GHz频段的网络
    wifi_scan_result_t *results = malloc(sizeof(wifi_scan_result_t) * ap_count_actual);
    if (results == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for filtered results");
        free(ap_records);
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, instance_scan_done);
        return NULL;
    }

    int valid_count = 0;
    for (int i = 0; i < ap_count_actual; i++)
    {
        if (is_2_4ghz_channel(ap_records[i].primary))
        {
            strncpy(results[valid_count].ssid, (char *)ap_records[i].ssid, sizeof(results[valid_count].ssid) - 1);
            results[valid_count].ssid[sizeof(results[valid_count].ssid) - 1] = '\0';
            results[valid_count].rssi = ap_records[i].rssi;
            results[valid_count].channel = ap_records[i].primary;
            valid_count++;
        }
    }

    ESP_LOGI(TAG, "Found %d 2.4GHz networks", valid_count);

    // ===================== 核心修改：SSID去重 =====================
    // 相同SSID仅保留信号最强的（RSSI数值越大，信号越强）
    int unique_count = 0;
    wifi_scan_result_t unique_results[valid_count]; // 临时存储去重后的结果

    for (int i = 0; i < valid_count; i++)
    {
        bool is_duplicate = false;
        // 遍历已去重的列表，检查当前SSID是否重复
        for (int j = 0; j < unique_count; j++)
        {
            if (strcmp(results[i].ssid, unique_results[j].ssid) == 0)
            {
                is_duplicate = true;
                // 如果当前SSID信号更强，替换原有记录
                if (results[i].rssi > unique_results[j].rssi)
                {
                    unique_results[j] = results[i];
                }
                break;
            }
        }
        // 非重复SSID，添加到去重列表
        if (!is_duplicate)
        {
            unique_results[unique_count++] = results[i];
        }
    }

    // 将去重后的结果覆盖原数组，更新有效数量
    memcpy(results, unique_results, sizeof(wifi_scan_result_t) * unique_count);
    valid_count = unique_count;
    ESP_LOGI(TAG, "Found %d unique 2.4GHz networks (after deduplication)", valid_count);
    // ==============================================================

    // 按信号强度排序（从强到弱）
    for (int i = 0; i < valid_count - 1; i++)
    {
        for (int j = 0; j < valid_count - i - 1; j++)
        {
            if (results[j].rssi < results[j + 1].rssi)
            {
                wifi_scan_result_t temp = results[j];
                results[j] = results[j + 1];
                results[j + 1] = temp;
            }
        }
    }

    // 计算JSON字符串所需的总长度
    // 基本格式: [{"ssid":"xxx","rssi":-xx},...]
    size_t total_len = 3; // 开头的"["和结尾的"]"

    for (int i = 0; i < valid_count; i++)
    {
        // 每个项目: {"ssid":"xxx","rssi":-xx}
        total_len += 12; // {"ssid":""
        total_len += strlen(results[i].ssid);
        total_len += 10; // ","rssi":

        // rssi值最多占5个字符（包括负号和结束符）
        char rssi_str[6];
        snprintf(rssi_str, sizeof(rssi_str), "%d", results[i].rssi);
        total_len += strlen(rssi_str);

        total_len += 1; // "}"

        if (i < valid_count - 1)
        {
            total_len += 1; // 逗号
        }
    }

    // 分配JSON字符串内存
    char *json_str = malloc(total_len + 1);
    if (json_str == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for JSON string");
        free(results);
        free(ap_records);
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, instance_scan_done);
        return NULL;
    }

    // 构建JSON字符串
    char *ptr = json_str;
    ptr += sprintf(ptr, "[");

    for (int i = 0; i < valid_count; i++)
    {
        // 开始一个网络对象
        ptr += sprintf(ptr, "{\"ssid\":\"%s\",\"rssi\":%d", results[i].ssid, results[i].rssi);

        // 添加结束符
        ptr += sprintf(ptr, "}");

        // 如果不是最后一个，添加逗号
        if (i < valid_count - 1)
        {
            ptr += sprintf(ptr, ",");
        }
    }

    ptr += sprintf(ptr, "]");

    // 清理资源
    free(results);
    free(ap_records);
    esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, instance_scan_done);

    ESP_LOGI(TAG, "WiFi scan completed, JSON length: %d bytes", strlen(json_str));

    return json_str;
}

// 清理JSON字符串的函数（如果调用者需要释放内存）
void wifi_scan_json_free(char *json_str)
{
    if (json_str)
    {
        free(json_str);
    }
}
char *json_result = NULL;
// 添加WiFi扫描的HTTP处理函数
static esp_err_t http_handler_scan(httpd_req_t *req)
{
    if (json_result == NULL)
    {
        const char *error_json = "[]";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, error_json, strlen(error_json));
        return ESP_OK;
    }

    // 发送JSON响应
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json_result, strlen(json_result));

    ESP_LOGI(TAG, "json_result sent: %s", json_result);

    return ESP_OK;
}

void start_ApSetWifi(void)
{
    // 调整日志级别，减少不必要的日志输出
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("wifi", ESP_LOG_WARN);

    // 初始化NVS
    // esp_err_t ret = nvs_flash_init();
    // if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    //     ESP_ERROR_CHECK(nvs_flash_erase());
    //     ret = nvs_flash_init();
    // }
    // ESP_ERROR_CHECK(ret);

    // 初始化SSID
    memset(AP_SSID, 0, sizeof(AP_SSID));
    memcpy(AP_SSID, deviceSN, strlen(deviceSN));

    // 初始化网络
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 创建事件组
    wifi_event_group = xEventGroupCreate();

    // 启动AP模式
    wifi_init_softap();

    json_result = wifi_scan_to_json();
    if (json_result != NULL)
    {
        ESP_LOGI(TAG, "WiFi Scan Results: %s", json_result);
        // free(wifi_scan_buf);
    }
    else
    {
        ESP_LOGW(TAG, "No WiFi scan results available");
    }

    // 启动Web服务器
    start_webserver();

    // 创建WiFi连接任务
    xTaskCreate(wifi_connect_task, "wifi_connect_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Network configuration system started");
    ESP_LOGI(TAG, "Please connect to WiFi: %s", AP_SSID);
    ESP_LOGI(TAG, "Then visit in browser: 192.168.4.1");
}