/*
 * STM32 HAL 适配层：把通用 OTA 状态机与具体开发板外设隔离。
 * 移植到另一块板时通常只改本文件和 ota_port_config.h；协议、Flash 状态机
 * 与校验算法无需知道 UART 句柄、复位方式或 LCD 状态模块的具体名称。
 */
#include "stm32f1xx_hal.h"
#include "ota_port_config.h"
#include "ota_port.h"
extern UART_HandleTypeDef OTA_UART_HANDLE;

bool OtaPort_ReadByte(uint8_t *byte)
{
    /* timeout=0 是非阻塞轮询：无数据时立即返回，不占住 OTA 任务。 */
    if (byte == 0) return false;
    return HAL_UART_Receive(&OTA_UART_HANDLE, byte, 1U, 0U) == HAL_OK;
}

void OtaPort_Send(const uint8_t *data, uint16_t length)
{
    /* 应答使用有限超时，串口异常时不会让整个 OTA 任务永久卡住。 */
    if ((data != 0) && (length != 0U))
        (void)HAL_UART_Transmit(&OTA_UART_HANDLE, (uint8_t *)data, length, 100U);
}

void OtaPort_ResetToBootloader(void)
{
    /* 软件复位后 CPU 从 Bootloader 起始地址重新执行，再依据元数据选槽。 */
    NVIC_SystemReset();
}

void OtaPort_HandleGatewayStatus(const char *line)
{
    /* 示例工程没有 LCD 状态页，因此明确忽略；真实工程可在这里转交业务层。 */
    /* 可选：业务工程可以在这里更新 LCD/状态页；OTA 核心本身不依赖它。 */
    (void)line;
}
