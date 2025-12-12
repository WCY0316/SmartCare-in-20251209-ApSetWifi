/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef WIFI_BLE_PROVISIONING_H
#define WIFI_BLE_PROVISIONING_H
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_mac.h"
#include <esp_wifi.h>
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_blufi_api.h"
#include "esp_blufi.h"
#include <unistd.h>
#include <errno.h>
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_err.h"

#pragma once

#define CONFIG_EXAMPLE_IPV4

#define EXAMPLE_WIFI_CONNECTION_MAXIMUM_RETRY CONFIG_EXAMPLE_WIFI_CONNECTION_MAXIMUM_RETRY
#define EXAMPLE_INVALID_REASON 255
#define EXAMPLE_INVALID_RSSI -128

#define WIFI_NAME_MAX_LEN 64
#define WIFI_PASSWORD_MAX_LEN 32

typedef struct
{
    char WifiSsid[WIFI_NAME_MAX_LEN];
    char WifiPassword[WIFI_PASSWORD_MAX_LEN];

    uint8_t ServerIP[4];
    uint16_t ServerPort;
} WIFI_USER_PARAM;

#define BLUFI_EXAMPLE_TAG "BLUFI_EXAMPLE"
#define BLUFI_INFO(fmt, ...) ESP_LOGI(BLUFI_EXAMPLE_TAG, fmt, ##__VA_ARGS__)
#define BLUFI_ERROR(fmt, ...) ESP_LOGE(BLUFI_EXAMPLE_TAG, fmt, ##__VA_ARGS__)

void blufi_dh_negotiate_data_handler(uint8_t *data, int len, uint8_t **output_data, int *output_len, bool *need_free);
int blufi_aes_encrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);
int blufi_aes_decrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);
uint16_t blufi_crc_checksum(uint8_t iv8, uint8_t *data, int len);

int blufi_security_init(void);
void blufi_security_deinit(void);
int esp_blufi_gap_register_callback(void);
esp_err_t esp_blufi_host_init(void);
esp_err_t esp_blufi_host_and_cb_init(esp_blufi_callbacks_t *callbacks);
esp_err_t esp_blufi_host_deinit(void);

int TcpSend(uint8_t *src, const uint32_t len);
int TcpClose(void);
int TcpCloseFlag(void);
int IsWifiConnected(void);
void ResetWiFiLinkState(void);
void InitUserWifi(void);
void blufi_entry_func(char *custom_device_name);
void StartAp(void);
#endif // WIFI_BLE_PROVISIONING_H