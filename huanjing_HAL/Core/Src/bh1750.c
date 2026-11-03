/*
 * BH1750 I2C 光照读取实现。
 * 初始化发送“上电”和“连续高分辨率”命令；之后 ReadLux 直接读取最新测量值。
 * HAL 使用包含读写位的 8 位地址，所以调用时要把数据手册中的 0x23 左移一位。
 */
#include "bh1750.h"
#include "i2c.h"

/* 7 位 I2C 地址及 BH1750 的两个基础命令。 */
#define BH1750_ADDRESS       0x23U
#define BH1750_POWER_ON      0x01U
#define BH1750_CONT_H_RES    0x10U
#define BH1750_TIMEOUT_MS    200U

bool Bh1750_Init(void)
{
  uint8_t command;

  /* 第一步：让传感器从掉电状态恢复。 */
  command = BH1750_POWER_ON;
  if (HAL_I2C_Master_Transmit(&hi2c1, BH1750_ADDRESS << 1U, &command, 1U,
                              BH1750_TIMEOUT_MS) != HAL_OK)
  {
    return false;
  }

  /* 第二步：连续高分辨率模式，之后直接读取两个数据字节即可。 */
  command = BH1750_CONT_H_RES;
  return HAL_I2C_Master_Transmit(&hi2c1, BH1750_ADDRESS << 1U, &command, 1U,
                                  BH1750_TIMEOUT_MS) == HAL_OK;
}

bool Bh1750_ReadLux(uint16_t *lux)
{
  uint8_t raw[2];
  uint16_t measurement;

  if (lux == NULL)
  {
    return false;
  }

  /* BH1750 一次输出两个字节，高字节在前。 */
  if (HAL_I2C_Master_Receive(&hi2c1, BH1750_ADDRESS << 1U, raw, 2U,
                             BH1750_TIMEOUT_MS) != HAL_OK)
  {
    return false;
  }

  measurement = (uint16_t)(((uint16_t)raw[0] << 8U) | raw[1]);
  /* 数据手册换算：lux = 原始值 / 1.2，改写为整数运算避免浮点数。 */
  *lux = (uint16_t)(((uint32_t)measurement * 10U) / 12U);
  return true;
}
