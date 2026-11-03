/* MQ135 模拟空气质量传感器接口。 */
#ifndef __MQ135_H__
#define __MQ135_H__

#include <stdbool.h>
#include <stdint.h>

/* 一次 ADC 采样的原始值及其简单估算值。 */
typedef struct
{
  uint16_t raw_adc;       /* ADC 原始值，范围通常为 0~4095。 */
  uint16_t estimated_ppm; /* 仅用于显示的线性估算值，不是标定后的真实 ppm。 */
} Mq135Data_t;

/*
 * 通过 ADC1 通道 10 读取一次并生成显示用估算值；data 不能为 NULL。
 * 成功返回 true。estimated_ppm 未经气体、温湿度和负载电阻标定，
 * 只能用于趋势显示，不能作为人身安全或法规测量依据。
 */
bool Mq135_Read(Mq135Data_t *data);

#endif
