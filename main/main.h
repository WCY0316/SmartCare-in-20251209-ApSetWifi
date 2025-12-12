// main.h
#ifndef _MAIN_H
#define _MAIN_H

#include <nvs_flash.h>

#define FIRMWARE_VERSION "0.0.9" // 软件版本号
// 系统状态机定义
typedef volatile enum {
    STA_INIT,     // 初始态，连接wifi等
    STA_IDLE,     // 空闲态
    STA_BLE,      // 蓝牙通信态
    STA_HEATING,  // 加热态
    STA_REACTING, // 反应态
    STA_PAUSE,    // 暂停态
    STA_DEBUG,    // 调试态
    STA_ERROR,

    STA_BACK_TO_IDLE, // 收尾动作，自动切换至IDLE
} MACHINE_STATE;

extern nvs_handle_t nvsHandle; // NVS句柄

int GetMState(void);
int SetMState(MACHINE_STATE newState);
char *GetMStateStr(MACHINE_STATE state);
char *DisposeStr(char *str, char *buf);

extern char deviceSN[11];
// 内容
#endif