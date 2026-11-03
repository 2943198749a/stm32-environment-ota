/* Slot B 与 Slot A 完全相同，唯一不同是链接地址、槽位宏和版本号。 */
#include "stm32f1xx_hal.h"
#include "ota_port_config.h"
#include "ota_app.h"
#include "ota_uart.h"

extern UART_HandleTypeDef huart2;
extern void SystemClock_Config(void);
extern void MX_GPIO_Init(void);
extern void MX_USART2_UART_Init(void);

int main(void)
{
    /* 运行逻辑与槽 A 相同，区别由链接地址、OTA_SLOT_B 和版本宏决定。 */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    SCB->VTOR = OTA_SLOT_B_ADDRESS;
    /* 初始化升级写入会话和串口接收状态机，声明当前正在槽 B 运行。 */
    OtaApp_Init();
    OtaUart_Init(OTA_SLOT_B);

    uint32_t bootTime = HAL_GetTick();
    bool confirmed = false;
    while (1)
    {
        /* 所有耗时协议处理都在主循环完成，UART 中断只负责缓存字节。 */
        OtaUart_Process();
        if ((!confirmed) && OtaApp_IsConfirmationRequired(OTA_SLOT_B) &&
            (HAL_GetTick() - bootTime >= 10000U))
        {
            if (OtaApp_ConfirmRunning(OTA_SLOT_B) == HAL_OK)
                confirmed = true;
        }
        /* 维持快速轮询，同时允许 HAL 的毫秒节拍继续推进。 */
        HAL_Delay(1U);
    }
}
