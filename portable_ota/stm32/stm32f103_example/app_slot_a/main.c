/*
 * 最小 Slot A 应用示例。
 * 真实工程可以在这个循环中加入传感器、LCD 和业务逻辑，OTA 调用保留即可。
 */
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
    /* 先让 HAL 建立系统节拍，再配置板级时钟、GPIO 和连接网关的 USART2。 */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    /* 把中断向量表指向 Slot A，而不是默认的 0x08000000。 */
    SCB->VTOR = OTA_SLOT_A_ADDRESS;
    /* 初始化应用写入状态和串口 OTA 状态机；当前编译目标明确是槽 A。 */
    OtaApp_Init();
    OtaUart_Init(OTA_SLOT_A);

    uint32_t bootTime = HAL_GetTick();
    bool confirmed = false;
    while (1)
    {
        /* OTA 必须持续处理 UART，否则 ESP32 发来的数据会堆积。 */
        /* 主循环不断消费网关字节；解析和 Flash 写入都在这里而不是中断中。 */
        OtaUart_Process();

        /* 新镜像启动后先观察 10 秒，确认没有立即崩溃再写 active。 */
        if ((!confirmed) && OtaApp_IsConfirmationRequired(OTA_SLOT_A) &&
            (HAL_GetTick() - bootTime >= 10000U))
        {
            if (OtaApp_ConfirmRunning(OTA_SLOT_A) == HAL_OK)
                confirmed = true;
        }
        /* 短暂让出 CPU，同时保持足够快的串口处理频率。 */
        HAL_Delay(1U);
    }
}
