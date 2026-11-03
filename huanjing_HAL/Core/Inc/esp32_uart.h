/*
 * ESP32-C3 串口传输层。
 *
 * 本模块只负责 USART2 的字节收发，不解析 ENV、STATUS 或 OTA 协议。
 * 接收中断把字节写入环形缓冲区，OTA 任务通过 Esp32Uart_ReadByte()
 * 依次取走字节，因此系统中只能有一个串口接收消费者。
 *
 * USART2 的波特率、引脚和中断由 CubeMX 生成代码初始化；本模块只管理
 * 应用层缓冲区。所有公开函数均应在任务上下文调用，接收回调除外。
 */
#ifndef __ESP32_UART_H
#define __ESP32_UART_H

#include <stdbool.h>
#include <stdint.h>

/*
 * 清空软件环形缓冲区并启动第一次单字节接收中断。
 * 调用前必须先执行 MX_USART2_UART_Init()；本函数不重复配置 USART2。
 */
void Esp32Uart_Init(void);

/*
 * 发送以 '\0' 结尾的字符串，最长阻塞 100 ms。
 * text 不能为 NULL；成功完整交给 HAL 时返回 true。
 * 不能在中断服务函数中调用。
 */
bool Esp32Uart_SendString(const char *text);

/*
 * 非阻塞读取一个已接收字节。
 * byte 不能为 NULL；读到数据返回 true，没有数据立即返回 false。
 * 只能由一个任务消费，否则两个任务会分走同一条协议字节流。
 */
bool Esp32Uart_ReadByte(uint8_t *byte);

#endif
