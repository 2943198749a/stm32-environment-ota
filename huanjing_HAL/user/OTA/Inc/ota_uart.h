/*
 * ESP32 Arduino 网关与 STM32 之间的 OTA 串口协议。
 *
 * 普通文字命令用于“开始”和“结束”；固件数据使用二进制帧，
 * 这样 bin 文件中的换行符、逗号等字节不会被误当成文本。
 *
 * 帧格式：[A5 5A][序号小端16位][长度小端16位][负载][CRC小端16位]。
 * CRC16/MODBUS 从序号开始计算到负载结束，不包含同步字节和 CRC 自身。
 * 长度必须为偶数，因为 STM32F1 Flash 以半字为最小编程单位。
 */
#ifndef OTA_UART_H
#define OTA_UART_H

#include <stdbool.h>
#include <stdint.h>
#include "ota_layout.h"

/* 数据帧固定开头：A5 5A。 */
#define OTA_UART_SYNC_1             0xA5U
#define OTA_UART_SYNC_2             0x5AU
/* 每一帧最多携带 512 字节，便于 STM32F1 分块写 Flash。 */
#define OTA_UART_MAX_PAYLOAD         512U
/* 同步字、序号、长度、CRC16 一共 8 字节。 */
#define OTA_UART_FRAME_OVERHEAD      8U

/*
 * 初始化 OTA 串口接收器；running_slot 是当前正在运行的应用槽位。
 * 必须在 USART2 环形缓冲区初始化后、OTA 任务启动前调用一次。
 */
void OtaUart_Init(OtaSlot_t running_slot);
/*
 * 非阻塞处理环形缓冲区中现有的全部字节。
 * 空闲时接收 STATUS/OTA_BEGIN 文本，升级时接收二进制帧及 OTA_END/OTA_ABORT。
 * 本函数会写 Flash，必须在 OTA 任务中调用，不能在 UART 中断中调用。
 */
void OtaUart_Process(void);
/* true 表示 STM32 正在接收并写入升级包。 */
bool OtaUart_IsBusy(void);
/* 返回简短状态文本，供 LCD 或电脑串口调试显示。 */
const char *OtaUart_GetLastMessage(void);

/* CRC16/MODBUS：ESP32 和 STM32 对同一段帧内容都使用它。 */
uint16_t OtaUart_Crc16(const uint8_t *data, uint32_t length);

#endif /* OTA_UART_H */
