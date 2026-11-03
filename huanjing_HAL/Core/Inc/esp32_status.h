/* ESP32 网关状态模块：解析 STATUS 文本，供 LCD 页面读取。 */
#ifndef __ESP32_STATUS_H
#define __ESP32_STATUS_H

#include <stdbool.h>
#include <stdint.h>

/* ESP32 状态文本中最后一个字段最多保存 15 个可见字符，再留一个 \0。 */
#define ESP32_STATUS_MESSAGE_SIZE 16U

typedef struct
{
    bool received;       /* STM32 是否至少收到过一条合法 STATUS。 */
    bool wifi_connected; /* ESP32 是否已连接 Wi-Fi。 */
    bool mqtt_connected; /* ESP32 是否已连接 MQTT。 */
    bool publish_ok;     /* ESP32 最近一次环境数据上报是否成功。 */
    bool ota_active;     /* ESP32 是否正在传输 OTA 固件。 */
    uint8_t ota_progress;/* OTA 传输进度：0~100。 */
    char message[ESP32_STATUS_MESSAGE_SIZE]; /* 例如 ready、downloading。 */
} Esp32Status_t;

/* 清空共享状态；使用短临界区，初始化后 LCD 会显示 ESP32: WAITING。 */
void Esp32Status_Init(void);

/*
 * 解析 STATUS,wifi,mqtt,publish,ota_active,progress,message。
 * 四个布尔字段只能为 0/1，progress 必须为 0~100，message 最多 15 字符。
 * line 不能为 NULL；格式合法时原子替换共享状态并返回 true。
 */
bool Esp32Status_ProcessLine(const char *line);

/* 使用短临界区返回最新状态副本；调用者获得独立值，可以在临界区外绘制 LCD。 */
Esp32Status_t Esp32Status_Get(void);

#endif
