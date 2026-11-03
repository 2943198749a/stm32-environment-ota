/* 仅供主机端传感器算法测试使用的 HAL 最小声明。 */
#ifndef STM32F1XX_HAL_H
#define STM32F1XX_HAL_H

#include <stdint.h>

/* 主机端编译测试使用的 PF8 引脚位掩码。 */
#define GPIO_PIN_8 (1U << 8)

uint32_t HAL_GetTick(void);

#endif
