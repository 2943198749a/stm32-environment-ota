/*
 * MQ135 ADC 读取实现。
 * 当前程序只读取 ADC 原始趋势，并线性映射到 0~1000 作为界面示意值。
 * 真正 ppm 计算还需要预热、负载电阻、洁净空气基准和温湿度补偿。
 */
#include "mq135.h"
#include "adc.h"

bool Mq135_Read(Mq135Data_t *data)
{
  uint32_t raw;

  if (data == NULL)
  {
    return false;
  }

  /* 启动 ADC、等待一次转换完成；超时说明本次数据无效。 */
  if ((HAL_ADC_Start(&hadc1) != HAL_OK) ||
      (HAL_ADC_PollForConversion(&hadc1, 100U) != HAL_OK))
  {
    return false;
  }

  raw = HAL_ADC_GetValue(&hadc1);
  (void)HAL_ADC_Stop(&hadc1);

  data->raw_adc = (uint16_t)raw;
  /* 未标定时只做 0~1000 的线性换算，方便 LCD 展示趋势。 */
  data->estimated_ppm = (uint16_t)((raw * 1000U) / 4095U);
  return true;
}
