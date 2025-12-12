#ifndef _BSP_LOG_H__
#define _BSP_LOG_H__
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>
#include "esp_task_wdt.h"
#include "esp_chip_info.h"
// #include "esp_heap_task_info.h"
#include "esp_err.h"
#include "freertos/event_groups.h"

void set_local_timezone(void);

void VTimeTask(void *arg);
void initialize_sntp(void);
void print_current_time(char *buf, size_t buf_size);
// esp_err_t init_spiffs(void);
esp_err_t init_log_storage(void);
static int find_available_log_file(void);
static void get_log_path(int index, char *path, size_t size);
void write_log(const char *level, const char *tag, const char *format, ...);
esp_err_t print_log_file(int index);
esp_err_t print_log_file_tcp(int index);
esp_err_t clear_all_log_files(void);
esp_err_t send_log_file_sizes(void);
esp_err_t print_all_log_file_sizes(void);
esp_err_t send_picbuf(void);
#endif
