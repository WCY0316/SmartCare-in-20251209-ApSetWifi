#ifndef _TCPCLIENT_H__
#define _TCPCLIENT_H__
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_task_wdt.h"
#include "esp_chip_info.h"
// #include "esp_heap_task_info.h"
#include "esp_partition.h"
#include "esp_err.h"
#include "freertos/event_groups.h"
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h> // struct addrinfo
#include <arpa/inet.h>
#include "timer.h"
// #include "esp_netif.h"

#define CONFIG_EXAMPLE_IPV4

// #define HOST_IP_ADDR "39.98.132.214"    //自用综合服务器
// #define HOST_DNS_NAME "admin.care.bailingkeji.com"
// #define HOST_IP_ADDR "8.141.21.154"                     // 动物实验服务器
// #define HOST_DNS_TEST "pet.010test.top"                 // wcy debug
// #define HOST_DNS_NAME_IATROLOGY "animalsocket.jyrchina.cn"        // 20250317 新增
// #define HOST_DNS_NAME_IATROLOGY "dltsocket.jyrchina.cn" // 20250414 新增，人医肠屏

// #define PORT 20002

#define ESP_FAILMUTEX -2

void VTcpClientTask(void *pvParameters);
int TcpClose(void);
int TcpCloseFlag(void);
int TcpSend(uint8_t *src, const uint32_t len);
int TcpSend_fen(uint8_t *data, const uint32_t data_len);
int SendStartTimeToServer(void);
int SendJpgToServer(uint8_t *PicData, uint32_t PicSize);
int SendHeartBeat(void);
int IsServerConnected(void);

extern volatile char *jyrHostDnsName;
extern volatile int jyrPort;

#endif
