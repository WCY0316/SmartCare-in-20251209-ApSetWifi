#ifndef _TCPSERVER_H__
#define _TCPSERVER_H__

#include <esp_system.h>
#include <sys/param.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <netdb.h> // struct addrinfo
#include <arpa/inet.h>
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_chip_info.h"
// #include "esp_heap_task_info.h"
#include "esp_err.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "lwip/ip4_addr.h"
#include "lwip/sockets.h"
#include <lwip/netdb.h>
#include "lwip/sys.h"
#include "lwip/err.h"
#include "lwip/prot/dhcp.h"
#include "apps/dhcpserver/dhcpserver.h"
#include "esp_mac.h"

void VTcpServerTask(void *pvParameters);
int TcpSendServer(uint8_t *src, const uint32_t len);
#endif
