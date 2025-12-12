/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/****************************************************************************
 * This is a demo for bluetooth config wifi connection to ap. You can config ESP32 to connect a softap
 * or config ESP32 as a softap to be connected by other device. APP can be downloaded from github
 * android source code: https://github.com/EspressifApp/EspBlufi
 * iOS source code: https://github.com/EspressifApp/EspBlufiForiOS
 ****************************************************************************/
#include "wifi_ble_provisioning.h"
#include "main.h"
#include "bsp_log.h"

// #define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
extern char deviceSN[]; // 设备名称
static const char *TAG = "wifi_ble_provisioning";

static WIFI_USER_PARAM sWifiUserParam;

// nvs_handle_t nvsHandle;
static bool sWifiParamValid = false;
static uint32_t ServerConnected = 0;
static uint32_t reconnect_count = 0;

static void example_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param);

static wifi_config_t sta_config;
static wifi_config_t ap_config;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

static uint8_t example_wifi_retry = 0;

/* store the station info for send back to phone */
static bool gl_sta_connected = false;
static bool gl_sta_got_ip = false;
static bool ble_is_connected = false;
static uint8_t gl_sta_bssid[6];
static uint8_t gl_sta_ssid[32];
static int gl_sta_ssid_len;
static wifi_sta_list_t gl_sta_list;
static bool gl_sta_is_connecting = false;
static esp_blufi_extra_info_t gl_sta_conn_info;
static bool s_user_link_state = false;

// 配置宏定义
#define AP_SSID "ESP32_AP_Demo" // AP热点名称
#define AP_PASSWORD "12345678"  // AP密码（至少8位）
#define AP_CHANNEL 6            // 信道
#define AP_MAX_CONN 4           // 最大连接数

// wifi连接状态
int IsWifiConnected(void)
{
    return (s_user_link_state ? 1 : 0);
}

void ResetWiFiLinkState(void)
{
    s_user_link_state = 0;
}

static void example_record_wifi_conn_info(int rssi, uint8_t reason)
{
    memset(&gl_sta_conn_info, 0, sizeof(esp_blufi_extra_info_t));
    if (gl_sta_is_connecting)
    {
        gl_sta_conn_info.sta_max_conn_retry_set = true;
        gl_sta_conn_info.sta_max_conn_retry = EXAMPLE_WIFI_CONNECTION_MAXIMUM_RETRY;
    }
    else
    {
        gl_sta_conn_info.sta_conn_rssi_set = true;
        gl_sta_conn_info.sta_conn_rssi = rssi;
        gl_sta_conn_info.sta_conn_end_reason_set = true;
        gl_sta_conn_info.sta_conn_end_reason = reason;
    }
}

static void example_wifi_connect(void)
{
    example_wifi_retry = 0;
    gl_sta_is_connecting = (esp_wifi_connect() == ESP_OK);
    example_record_wifi_conn_info(EXAMPLE_INVALID_RSSI, EXAMPLE_INVALID_REASON);
    
}

static bool example_wifi_reconnect(void)
{
    bool ret;
    if (gl_sta_is_connecting && example_wifi_retry++ < EXAMPLE_WIFI_CONNECTION_MAXIMUM_RETRY)
    {
        BLUFI_INFO("BLUFI WiFi starts reconnection\n");
        gl_sta_is_connecting = (esp_wifi_connect() == ESP_OK);
        example_record_wifi_conn_info(EXAMPLE_INVALID_RSSI, EXAMPLE_INVALID_REASON);
        ret = true;
    }
    else
    {
        ret = false;
    }
    return ret;
}

static int softap_get_current_connection_number(void)
{
    esp_err_t ret;
    ret = esp_wifi_ap_get_sta_list(&gl_sta_list);
    if (ret == ESP_OK)
    {
        return gl_sta_list.num;
    }

    return 0;
}

static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    wifi_mode_t mode;

    switch (event_id)
    {
    case IP_EVENT_STA_GOT_IP:
    {
        esp_blufi_extra_info_t info;

        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        esp_wifi_get_mode(&mode);

        memset(&info, 0, sizeof(esp_blufi_extra_info_t));
        memcpy(info.sta_bssid, gl_sta_bssid, 6);
        info.sta_bssid_set = true;
        info.sta_ssid = gl_sta_ssid;
        info.sta_ssid_len = gl_sta_ssid_len;
        gl_sta_got_ip = true;
        if (ble_is_connected == true)
        {
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, softap_get_current_connection_number(), &info);
        }
        else
        {
            BLUFI_INFO("BLUFI BLE is not connected yet\n");
        }
        break;
    }
    default:
        break;
    }
    return;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    wifi_event_sta_connected_t *event;
    wifi_event_sta_disconnected_t *disconnected_event;
    wifi_mode_t mode;

    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        example_wifi_connect();
        break;
    case WIFI_EVENT_STA_CONNECTED:
        gl_sta_connected = true;
        s_user_link_state = gl_sta_connected;
        gl_sta_is_connecting = false;
        event = (wifi_event_sta_connected_t *)event_data;
        memcpy(gl_sta_bssid, event->bssid, 6);
        memcpy(gl_sta_ssid, event->ssid, event->ssid_len);
        gl_sta_ssid_len = event->ssid_len;
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        /* Only handle reconnection during connecting */
        // if (gl_sta_connected == false && example_wifi_reconnect() == false) {
        //     gl_sta_is_connecting = false;
        //     disconnected_event = (wifi_event_sta_disconnected_t*) event_data;
        //     example_record_wifi_conn_info(disconnected_event->rssi, disconnected_event->reason);
        // }
        // /* This is a workaround as ESP32 WiFi libs don't currently
        //    auto-reassociate. */
        // gl_sta_connected = false;
        // s_user_link_state = gl_sta_connected;
        // gl_sta_got_ip = false;
        // memset(gl_sta_ssid, 0, 32);
        // memset(gl_sta_bssid, 0, 6);
        // gl_sta_ssid_len = 0;
        // xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        // BLUFI_INFO("Wifi STA Disconnected\n");

        // if (reconnect_count < 5)     //自动重连不超过5次
        {
            // ESP_LOGI(TAG, "Wi-Fi disconnected, trying to reconnect... (%ld)", reconnect_count + 1);
            reconnect_count++;
            if (esp_wifi_connect() == ESP_OK)
            {
                // ESP_LOGI(TAG, "Wi-Fi reconnected successfully.");
                reconnect_count = 0; // Reset the counter after reaching the limit
            }
        }
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        gl_sta_connected = false;
        s_user_link_state = gl_sta_connected;
        gl_sta_got_ip = false;
        memset(gl_sta_ssid, 0, 32);
        memset(gl_sta_bssid, 0, 6);
        gl_sta_ssid_len = 0;
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        // BLUFI_INFO("Wifi STA Disconnected\n");
        break;
    case WIFI_EVENT_AP_START:
        esp_wifi_get_mode(&mode);

        /* TODO: get config or information of softap, then set to report extra_info */
        if (ble_is_connected == true)
        {
            if (gl_sta_connected)
            {
                esp_blufi_extra_info_t info;
                memset(&info, 0, sizeof(esp_blufi_extra_info_t));
                memcpy(info.sta_bssid, gl_sta_bssid, 6);
                info.sta_bssid_set = true;
                info.sta_ssid = gl_sta_ssid;
                info.sta_ssid_len = gl_sta_ssid_len;
                esp_blufi_send_wifi_conn_report(mode, gl_sta_got_ip ? ESP_BLUFI_STA_CONN_SUCCESS : ESP_BLUFI_STA_NO_IP, softap_get_current_connection_number(), &info);
            }
            else if (gl_sta_is_connecting)
            {
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONNECTING, softap_get_current_connection_number(), &gl_sta_conn_info);
            }
            else
            {
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, softap_get_current_connection_number(), &gl_sta_conn_info);
            }
        }
        else
        {
            BLUFI_INFO("BLUFI BLE is not connected yet2\n");
        }
        break;
    case WIFI_EVENT_SCAN_DONE:
    {
        uint16_t apCount = 0;
        esp_wifi_scan_get_ap_num(&apCount);
        if (apCount == 0)
        {
            BLUFI_INFO("Nothing AP found");
            break;
        }
        wifi_ap_record_t *ap_list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);
        if (!ap_list)
        {
            BLUFI_ERROR("malloc error, ap_list is NULL");
            break;
        }
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, ap_list));
        esp_blufi_ap_record_t *blufi_ap_list = (esp_blufi_ap_record_t *)malloc(apCount * sizeof(esp_blufi_ap_record_t));
        if (!blufi_ap_list)
        {
            if (ap_list)
            {
                free(ap_list);
            }
            BLUFI_ERROR("malloc error, blufi_ap_list is NULL");
            break;
        }
        for (int i = 0; i < apCount; ++i)
        {
            blufi_ap_list[i].rssi = ap_list[i].rssi;
            memcpy(blufi_ap_list[i].ssid, ap_list[i].ssid, sizeof(ap_list[i].ssid));
        }

        if (ble_is_connected == true)
        {
            esp_blufi_send_wifi_list(apCount, blufi_ap_list);
        }
        else
        {
            BLUFI_INFO("BLUFI BLE is not connected yet3\n");
        }

        esp_wifi_scan_stop();
        free(ap_list);
        free(blufi_ap_list);
        break;
    }
    case WIFI_EVENT_AP_STACONNECTED:
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        BLUFI_INFO("station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
        break;
    }
    case WIFI_EVENT_AP_STADISCONNECTED:
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        BLUFI_INFO("station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
        break;
    }

    default:
        break;
    }
    return;
}

static esp_blufi_callbacks_t example_callbacks = {
    .event_cb = example_event_callback,
    .negotiate_data_handler = blufi_dh_negotiate_data_handler,
    .encrypt_func = blufi_aes_encrypt,
    .decrypt_func = blufi_aes_decrypt,
    .checksum_func = blufi_crc_checksum,
};

void UserDataAck(void)
{
    uint8_t buf[] = "SmartCare Project! 23.9.17\n";

    if (ble_is_connected == false)
        return;

    esp_blufi_send_custom_data(buf, sizeof(buf));
}
// blufi回调函数
static void example_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param)
{
    /* actually, should post to blufi_task handle the procedure,
     * now, as a example, we do it more simply */
    switch (event)
    {
    case ESP_BLUFI_EVENT_INIT_FINISH:
        BLUFI_INFO("BLUFI init finish\n");

        esp_blufi_adv_start();
        // esp_ble_gap_set_device_name("SmartCare1212");
        // esp_ble_gap_config_adv_data(&blufi_adv_data);
        break;
    case ESP_BLUFI_EVENT_DEINIT_FINISH:
        BLUFI_INFO("BLUFI deinit finish\n");
        break;
    case ESP_BLUFI_EVENT_BLE_CONNECT:
        BLUFI_INFO("BLUFI ble connect\n");
        ble_is_connected = true;
        esp_blufi_adv_stop();
        blufi_security_init();
        break;
    case ESP_BLUFI_EVENT_BLE_DISCONNECT:
        BLUFI_INFO("BLUFI ble disconnect\n");
        ble_is_connected = false;
        blufi_security_deinit();
        esp_blufi_adv_start();
        break;
    case ESP_BLUFI_EVENT_SET_WIFI_OPMODE:
        BLUFI_INFO("BLUFI Set WIFI opmode %d\n", param->wifi_mode.op_mode);
        ESP_ERROR_CHECK(esp_wifi_set_mode(param->wifi_mode.op_mode));
        break;
    case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
        BLUFI_INFO("BLUFI requset wifi connect to AP\n");
        /* there is no wifi callback when the device has already connected to this wifi
        so disconnect wifi before connection.
        */
        esp_wifi_disconnect();
        example_wifi_connect();
        break;
    case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
        BLUFI_INFO("BLUFI requset wifi disconnect from AP\n");
        esp_wifi_disconnect();
        break;
    case ESP_BLUFI_EVENT_REPORT_ERROR:
        BLUFI_ERROR("BLUFI report error, error code %d\n", param->report_error.state);
        esp_blufi_send_error_info(param->report_error.state);
        break;
    case ESP_BLUFI_EVENT_GET_WIFI_STATUS:
    {
        wifi_mode_t mode;
        esp_blufi_extra_info_t info;

        esp_wifi_get_mode(&mode);

        if (gl_sta_connected)
        {
            memset(&info, 0, sizeof(esp_blufi_extra_info_t));
            memcpy(info.sta_bssid, gl_sta_bssid, 6);
            info.sta_bssid_set = true;
            info.sta_ssid = gl_sta_ssid;
            info.sta_ssid_len = gl_sta_ssid_len;
            esp_blufi_send_wifi_conn_report(mode, gl_sta_got_ip ? ESP_BLUFI_STA_CONN_SUCCESS : ESP_BLUFI_STA_NO_IP, softap_get_current_connection_number(), &info);
        }
        else if (gl_sta_is_connecting)
        {
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONNECTING, softap_get_current_connection_number(), &gl_sta_conn_info);
        }
        else
        {
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, softap_get_current_connection_number(), &gl_sta_conn_info);
        }
        BLUFI_INFO("BLUFI get wifi status from AP\n");

        break;
    }
    case ESP_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE:
        BLUFI_INFO("blufi close a gatt connection");
        esp_blufi_disconnect();
        break;
    case ESP_BLUFI_EVENT_DEAUTHENTICATE_STA:
        /* TODO */
        BLUFI_INFO("BLUFI BLE 222\n");
        break;
    case ESP_BLUFI_EVENT_RECV_STA_BSSID:
        memcpy(sta_config.sta.bssid, param->sta_bssid.bssid, 6);
        sta_config.sta.bssid_set = 1;
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        BLUFI_INFO("Recv STA BSSID %s\n", sta_config.sta.ssid);
        break;
    case ESP_BLUFI_EVENT_RECV_STA_SSID:
        strncpy((char *)sta_config.sta.ssid, (char *)param->sta_ssid.ssid, param->sta_ssid.ssid_len);
        sta_config.sta.ssid[param->sta_ssid.ssid_len] = '\0';
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        BLUFI_INFO("Recv STA SSID %s\n", sta_config.sta.ssid);
        nvs_set_str(nvsHandle, "wifi_ssid", (char *)sta_config.sta.ssid);
        break;
    case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
        strncpy((char *)sta_config.sta.password, (char *)param->sta_passwd.passwd, param->sta_passwd.passwd_len);
        sta_config.sta.password[param->sta_passwd.passwd_len] = '\0';
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        BLUFI_INFO("Recv STA PASSWORD %s\n", sta_config.sta.password);
        nvs_set_str(nvsHandle, "wifi_passwd", (char *)sta_config.sta.password);
        break;
    case ESP_BLUFI_EVENT_RECV_SOFTAP_SSID:
        strncpy((char *)ap_config.ap.ssid, (char *)param->softap_ssid.ssid, param->softap_ssid.ssid_len);
        ap_config.ap.ssid[param->softap_ssid.ssid_len] = '\0';
        ap_config.ap.ssid_len = param->softap_ssid.ssid_len;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        BLUFI_INFO("Recv SOFTAP SSID %s, ssid len %d\n", ap_config.ap.ssid, ap_config.ap.ssid_len);
        break;
    case ESP_BLUFI_EVENT_RECV_SOFTAP_PASSWD:
        strncpy((char *)ap_config.ap.password, (char *)param->softap_passwd.passwd, param->softap_passwd.passwd_len);
        ap_config.ap.password[param->softap_passwd.passwd_len] = '\0';
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        BLUFI_INFO("Recv SOFTAP PASSWORD %s len = %d\n", ap_config.ap.password, param->softap_passwd.passwd_len);
        break;
    case ESP_BLUFI_EVENT_RECV_SOFTAP_MAX_CONN_NUM:
        if (param->softap_max_conn_num.max_conn_num > 4)
        {
            return;
        }
        ap_config.ap.max_connection = param->softap_max_conn_num.max_conn_num;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        BLUFI_INFO("Recv SOFTAP MAX CONN NUM %d\n", ap_config.ap.max_connection);
        break;
    case ESP_BLUFI_EVENT_RECV_SOFTAP_AUTH_MODE:
        if (param->softap_auth_mode.auth_mode >= WIFI_AUTH_MAX)
        {
            return;
        }
        ap_config.ap.authmode = param->softap_auth_mode.auth_mode;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        BLUFI_INFO("Recv SOFTAP AUTH MODE %d\n", ap_config.ap.authmode);
        break;
    case ESP_BLUFI_EVENT_RECV_SOFTAP_CHANNEL:
        if (param->softap_channel.channel > 13)
        {
            return;
        }
        ap_config.ap.channel = param->softap_channel.channel;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        BLUFI_INFO("Recv SOFTAP CHANNEL %d\n", ap_config.ap.channel);
        break;
    case ESP_BLUFI_EVENT_GET_WIFI_LIST:
    {
        wifi_scan_config_t scanConf = {
            .ssid = NULL,
            .bssid = NULL,
            .channel = 0,
            .show_hidden = false};
        esp_err_t ret = esp_wifi_scan_start(&scanConf, true);
        if (ret != ESP_OK)
        {
            esp_blufi_send_error_info(ESP_BLUFI_WIFI_SCAN_FAIL);
        }
        break;
    }
    case ESP_BLUFI_EVENT_RECV_CUSTOM_DATA:
        BLUFI_INFO("Recv Custom Data %" PRIu32 "\n", param->custom_data.data_len);
        esp_log_buffer_hex("Custom Data", param->custom_data.data, param->custom_data.data_len);
        UserDataAck();
        break;
    case ESP_BLUFI_EVENT_RECV_USERNAME:
        /* Not handle currently */
        BLUFI_INFO("Recv UserName\n");
        break;
    case ESP_BLUFI_EVENT_RECV_CA_CERT:
        /* Not handle currently */
        BLUFI_INFO("Recv CA_CERT\n");
        break;
    case ESP_BLUFI_EVENT_RECV_CLIENT_CERT:
        /* Not handle currently */
        break;
    case ESP_BLUFI_EVENT_RECV_SERVER_CERT:
        /* Not handle currently */
        BLUFI_INFO("Recv ServerCERT\n");
        break;
    case ESP_BLUFI_EVENT_RECV_CLIENT_PRIV_KEY:
        /* Not handle currently */
        break;
    case ESP_BLUFI_EVENT_RECV_SERVER_PRIV_KEY:
        /* Not handle currently */
        BLUFI_INFO("Recv ServerPrivKey\n");
        break;
    default:
        break;
    }
}
static void initialise_wifi(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    assert(ap_netif);
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    example_record_wifi_conn_info(EXAMPLE_INVALID_RSSI, EXAMPLE_INVALID_REASON);
    ESP_ERROR_CHECK(esp_wifi_start());
}
void StartAp(void)
{
    wifi_mode_t current_mode;
    esp_wifi_get_mode(&current_mode);

    if (current_mode == WIFI_MODE_STA)
    {
        // 若为AP+STA模式，切换为STA模式
        esp_wifi_set_mode(WIFI_MODE_APSTA);
    }

    // 配置AP参数
    wifi_config_t ap_config = {
        .ap = {
            .ssid = AP_SSID,
            .password = AP_PASSWORD,
            .ssid_len = strlen(AP_SSID),
            .channel = AP_CHANNEL,
            .max_connection = AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false}}};
    // 将deviceSN作为AP名称
    memcpy(ap_config.ap.ssid, deviceSN, strlen(deviceSN));
    ap_config.ap.ssid_len = strlen(deviceSN);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}
void InitUserWifi(void)
{
    esp_err_t ret;

    initialise_wifi();
    size_t required_size;
    nvs_get_str(nvsHandle, "wifi_ssid", NULL, &required_size);
    if (required_size < WIFI_NAME_MAX_LEN)
        nvs_get_str(nvsHandle, "wifi_ssid", sWifiUserParam.WifiSsid, &required_size);
    else
    {
        printf("Error!wifi_ssid too long!%d\n", required_size);
        return;
    }

    nvs_get_str(nvsHandle, "wifi_passwd", NULL, &required_size);
    if (required_size < WIFI_PASSWORD_MAX_LEN)
        nvs_get_str(nvsHandle, "wifi_passwd", sWifiUserParam.WifiPassword, &required_size);
    else
    {
        printf("Error!wifi_passwd too long!%d\n", required_size);
        return;
    }

    printf("wifi ssid:%s\n", sWifiUserParam.WifiSsid);
    printf("wifi passwd:%s\n", sWifiUserParam.WifiPassword);
    // // 配置AP参数
    // wifi_config_t ap_config = {
    //     .ap = {
    //         .ssid = AP_SSID,
    //         .password = AP_PASSWORD,
    //         .ssid_len = strlen(AP_SSID),
    //         .channel = AP_CHANNEL,
    //         .max_connection = AP_MAX_CONN,
    //         .authmode = WIFI_AUTH_WPA2_PSK,
    //         .pmf_cfg = {
    //             .required = false}}};
    // ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    wifi_config_t sta_config = {
        .sta = {
            //.ssid = EXAMPLE_ESP_WIFI_SSID,
            //.password = EXAMPLE_ESP_WIFI_PASS,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH, // ESP_WIFI_SAE_MODE,
            //.sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };
    memcpy(sta_config.sta.ssid, sWifiUserParam.WifiSsid, strlen(sWifiUserParam.WifiSsid));
    memcpy(sta_config.sta.password, sWifiUserParam.WifiPassword, strlen(sWifiUserParam.WifiPassword));

    // printf("Link WIFI directly\n");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}
// 在广播数据中显式包含设备名称
static uint8_t raw_adv_data[19] = {
    0x02, 0x01, 0x06,       // Flags（固定）
    0x03, 0x03, 0xF0, 0x18, // 服务UUID（固定）
    0x0B, 0x09,             // 动态计算名称字段头
    '0', '0', '0', '0', '0', '0', '0', '0', '0', '0'};
static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20, // 20ms
    .adv_int_max = 0x40, // 40ms
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};
// 全局标记广告数据配置状态
static bool adv_data_configured = false;

void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        if (param->adv_data_cmpl.status == ESP_BT_STATUS_SUCCESS)
        {
            adv_data_configured = true;
            esp_ble_gap_start_advertising(&adv_params);
        }
        else
        {
            ESP_LOGE("TAG", "设置广播数据失败: %d", param->adv_data_cmpl.status);
        }
        break;
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT: // 新增事件处理
        if (param->adv_data_cmpl.status == ESP_BT_STATUS_SUCCESS)
        {
            // 启动广播（如果尚未启动）
            if (!adv_data_configured)
            {
                esp_ble_gap_start_advertising(&adv_params);
            }
        }
        else
        {
            ESP_LOGE("TAG", "设置广播数据（新API）失败: %d", param->adv_data_cmpl.status);
        }
        break;
    default:
        break;
    }
}
void blufi_entry_func(char *custom_device_name)
{
    esp_err_t ret;

    memset(&raw_adv_data[9], ' ', 10);                    // 清空原有名称区域
    memcpy(&raw_adv_data[9], deviceSN, strlen(deviceSN)); // 注入新名称
    // int raw_len = sizeof((char *)raw_adv_data);
    // printf("raw_len : %d\n", raw_len);
    for (int i = 0; i < 19; i++)
    {
        printf("%02X ", raw_adv_data[i]);
    }

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret)
    {
        BLUFI_ERROR("%s initialize bt controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret)
    {
        BLUFI_ERROR("%s enable bt controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    // vTaskDelay(pdMS_TO_TICKS(200));

    ret = esp_blufi_host_and_cb_init(&example_callbacks);
    if (ret)
    {
        BLUFI_ERROR("%s initialise failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    // ========== 使用 ESP-IDF 标准 BLE 接口设置设备名称 ==========
    // vTaskDelay(pdMS_TO_TICKS(200));
    ret = esp_ble_gap_set_device_name(deviceSN); // 改用蓝牙核心接口
    if (ret == ESP_OK)
    {
        BLUFI_INFO("BLE device name set to: %s\n", deviceSN);
    }
    else
    {
        BLUFI_ERROR("Set device name failed: %s\n", esp_err_to_name(ret));
    }
    // vTaskDelay(pdMS_TO_TICKS(200));
    // ========================================================
    esp_ble_gap_register_callback(gap_event_handler); // 注册回调

    esp_err_t err = esp_ble_gap_config_adv_data_raw((uint8_t *)raw_adv_data, sizeof(raw_adv_data));
    if (err != ESP_OK)
    {
        printf("Advertising data configuration failed, error code: 0x%x\n", err);
        return;
    }
    // vTaskDelay(pdMS_TO_TICKS(200));
    // err = esp_ble_gap_stop_advertising();
    // if (err != ESP_OK)
    // {
    //     printf("Stop advertising failed, error code: 0x%x\n", err);
    //     return;
    // }
    // vTaskDelay(pdMS_TO_TICKS(200));
    // err = esp_ble_gap_start_advertising(&adv_params);
    // if (err != ESP_OK)
    // {
    //     printf("Start advertising failed, error code: 0x%x\n", err);
    //     return;
    // }

    BLUFI_INFO("BLUFI VERSION %04x\n", esp_blufi_get_version());
    write_log("INFO", TAG, "BLUFI Opened!");
}
