/*
 * STM32 HAL 适配层：把通用 OTA 状态机与本块板卡的外设隔离。
 * 如果以后把 OTA 核心移植到另一块 STM32 板，通常只需修改本文件和
 * ota_port_config.h，而不用改协议解析、Flash 状态机或 SHA-256 实现。
 */
#include "stm32f1xx_hal.h"
#include "ota_port_config.h"
#include "ota_port.h"
#include "esp32_status.h"
#include "esp32_uart.h"
extern UART_HandleTypeDef OTA_UART_HANDLE;

/**
 * @brief 从 ESP32 串口中断维护的环形缓冲区取出一个字节。
 * byte 为接收结果的输出指针，参数及无数据行为由 Esp32Uart_ReadByte 处理。
 * 返回底层读取结果，true 表示取得字节；非阻塞，由任务调用，不直接轮询 UART 硬件。
 */
bool OtaPort_ReadByte(uint8_t *byte)
{
    /*
     * UART 中断负责把字节放入环形缓冲区；
     * OTA 任务从缓冲区取数据，不再直接轮询硬件 USART2。
     */
    return Esp32Uart_ReadByte(byte);
}
/**
 * @brief 通过配置的 UART 同步发送 OTA 应答数据。
 * data 指向 length 字节；空指针或零长度时直接忽略；无返回值。
 * 使用 100ms 有限超时，底层发送错误不向上传递；应在任务上下文调用。
 */
void OtaPort_Send(const uint8_t *data, uint16_t length)
{
    /* 发送应答使用有限超时，避免串口故障让 OTA 任务永久卡住。 */
    if ((data != 0) && (length != 0U))
        (void)HAL_UART_Transmit(&OTA_UART_HANDLE, (uint8_t *)data, length, 100U);
}

/**
 * @brief 触发 Cortex-M 系统软件复位，使启动流程重新进入 Bootloader。
 * 无参数、无返回值；正常复位后不会继续执行本次调用后面的代码。
 * 调用前应完成启动记录保存及串口应答；下次启动槽位由 Bootloader 根据记录选择。
 */
void OtaPort_ResetToBootloader(void)
{
    /* 复位会重新进入地址 0x08000000 的 Bootloader，由元数据选择槽位。 */
    NVIC_SystemReset();
}

/**
 * @brief 将网关 STATUS 文本转交业务层，更新网关或显示状态。
 * line 为完整且以结束符结尾的文本，解析及合法性检查由 Esp32Status_ProcessLine 负责。
 * 无返回值，忽略业务解析结果；该适配点使通用 OTA 协议代码无需直接操作 LCD。
 */
void OtaPort_HandleGatewayStatus(const char *line)
{
    /* 状态文本交给业务层解析；通用 OTA 模块不直接依赖 LCD 页面。 */
    /* 可选：业务工程可以在这里更新 LCD/状态页；OTA 核心本身不依赖它。 */
    (void)Esp32Status_ProcessLine(line);
}
