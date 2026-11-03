/*
 * ESP32 网关状态模块实现。
 *
 * OTA 任务收到 STATUS 文本后调用 ProcessLine；UI 任务调用 Get 获得一份值拷贝。
 * 解析先写局部 next_status，只有整行字段全部合法才在短临界区内替换全局状态，
 * 因而 LCD 不会看到“只更新了一半”的结构体。
 */
#include "esp32_status.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <string.h>

/* UART 任务写入，LCD 任务读取的最新状态。 */
static Esp32Status_t g_status;

void Esp32Status_Init(void)
{
    /* 临界区与后续读写规则一致，也使将来重复初始化时保持线程安全。 */
    taskENTER_CRITICAL();
    (void)memset(&g_status, 0, sizeof(g_status));
    taskEXIT_CRITICAL();
}

bool Esp32Status_ProcessLine(const char *line)
{
    Esp32Status_t next_status;
    unsigned int wifi;
    unsigned int mqtt;
    unsigned int publish;
    unsigned int ota_active;
    unsigned int progress;
    int field_count;

    if (line == NULL)
    {
        return false;
    }
    //memset，清空
    (void)memset(&next_status, 0, sizeof(next_status));

    /* %15[^,\r\n] 可防止 message 写出数组，也不允许 message 再带逗号。 */
    //sscanf，解析字符串
    field_count = sscanf(line, "STATUS,%u,%u,%u,%u,%u,%15[^,\r\n]",
                         &wifi, &mqtt, &publish, &ota_active, &progress,
                         next_status.message);

    if ((field_count != 6) || (wifi > 1U) || (mqtt > 1U) ||
        (publish > 1U) || (ota_active > 1U) || (progress > 100U) ||
        (next_status.message[0] == '\0'))
    {
        return false;
    }

    next_status.received = true;
    next_status.wifi_connected = (wifi != 0U);
    next_status.mqtt_connected = (mqtt != 0U);
    next_status.publish_ok = (publish != 0U);
    next_status.ota_active = (ota_active != 0U);
    next_status.ota_progress = (uint8_t)progress;

    /* 结构体整体替换的时间很短，临界区只保护这一小段复制。 */
    taskENTER_CRITICAL();
    g_status = next_status;
    taskEXIT_CRITICAL();
    return true;
}

Esp32Status_t Esp32Status_Get(void)
{
    Esp32Status_t status;

    /* 返回结构体副本；临界区退出后调用者可安全、独立地格式化和显示。 */
    taskENTER_CRITICAL();
    status = g_status;
    taskEXIT_CRITICAL();
    return status;
}
