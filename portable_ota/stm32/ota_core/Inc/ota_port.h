/*
 * OTA 核心与具体 STM32 开发板之间的最小适配层。
 * 只有本模块知道实际 UART 缓冲区、复位指令和业务状态接收器；通用 OTA
 * 状态机不直接依赖 huart2、NVIC 或 LCD。以下接口均由 OTA 任务调用。
 */
#ifndef OTA_PORT_H
#define OTA_PORT_H
#include <stdbool.h>
#include <stdint.h>
/* 非阻塞读取一个 UART 字节；byte 不能为 NULL，无数据时返回 false。 */
bool OtaPort_ReadByte(uint8_t *byte);
/* 同步发送 length 个字节，空指针或零长度会被忽略；最长等待 100 ms。 */
void OtaPort_Send(const uint8_t *data, uint16_t length);
/* 请求 MCU 系统复位，使 Bootloader 根据 pending 元数据选择下一槽位。 */
void OtaPort_ResetToBootloader(void);
/* 把一条完整 STATUS 文本交给业务状态模块；line 的所有权仍属于调用者。 */
void OtaPort_HandleGatewayStatus(const char *line);
#endif
