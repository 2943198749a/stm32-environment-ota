/*
 * DHT11 单总线读取实现。所有微秒时序只放在本文件。
 * 一次交易由“主机拉低启动 -> 传感器应答 -> 读取 40 位 -> 校验和”组成。
 * DHT11 没有独立时钟线，0/1 由高电平持续时间表示，因此普通毫秒延时不够精确，
 * 这里使用 Cortex-M3 的 DWT 周期计数器完成微秒等待。
 */
#include "dht11.h"
#include "main.h"

/* DHT11 数据线接在 PC3；引脚配置本身由 CubeMX 的 gpio.c 提供时钟。 */
#define DHT11_PORT GPIOC
#define DHT11_PIN  GPIO_PIN_3

/* 将系统时钟频率换算为 DWT 计数器每微秒需要增加的周期数。 */
static uint32_t Dht11_CyclesPerUs(void)
{
  return HAL_RCC_GetHCLKFreq() / 1000000U;
}

/* 使用 Cortex-M3 的 DWT 周期计数器实现微秒延时，供 DHT11 时序使用。 */
static void Dht11_DelayUs(uint32_t us)
{
  uint32_t start = DWT->CYCCNT;
  uint32_t cycles = us * Dht11_CyclesPerUs();

  while ((uint32_t)(DWT->CYCCNT - start) < cycles)
  {
  }
}

/* 等待数据线变成指定电平；超过 timeout_us 仍未出现则说明本次读取失败。 */
static bool Dht11_WaitFor(GPIO_PinState state, uint32_t timeout_us)
{
  uint32_t start = DWT->CYCCNT;
  uint32_t timeout_cycles = timeout_us * Dht11_CyclesPerUs();

  while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) != state)
  {
    if ((uint32_t)(DWT->CYCCNT - start) >= timeout_cycles)
    {
      return false;
    }
  }

  return true;
}

/* 主机发送开始信号时，PC3 需要临时作为推挽输出。 */
static void Dht11_SetOutput(void)
{
  GPIO_InitTypeDef gpio = {0};

  gpio.Pin = DHT11_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DHT11_PORT, &gpio);
}

/* DHT11 回传 40 位数据前，PC3 必须切回带上拉的输入模式。 */
static void Dht11_SetInput(void)
{
  GPIO_InitTypeDef gpio = {0};

  gpio.Pin = DHT11_PIN;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(DHT11_PORT, &gpio);
}

void Dht11_Init(void)
{
  /* 打开 DWT 周期计数器，后续 Dht11_DelayUs 才能做微秒级延时。 */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  /* 空闲时由传感器驱动数据线，MCU 保持输入状态。 */
  Dht11_SetInput();
}

bool Dht11_Read(Dht11Data_t *data)
{
  uint8_t bytes[5] = {0};
  uint8_t bit;
  uint8_t index;

  if (data == NULL)
  {
    return false;
  }

  /* 主机拉低至少 18 ms，通知 DHT11 开始一次测量。 */
  Dht11_SetOutput();
  HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
  HAL_Delay(18U);
  HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
  Dht11_DelayUs(30U);
  Dht11_SetInput();

  /* 传感器应答低、高、低三个阶段，任一步超时都直接报失败。 */
  if (!Dht11_WaitFor(GPIO_PIN_RESET, 100U) ||
      !Dht11_WaitFor(GPIO_PIN_SET, 100U) ||
      !Dht11_WaitFor(GPIO_PIN_RESET, 100U))
  {
    return false;
  }

  /* DHT11 连续发送 40 位：湿度整数、小数、温度整数、小数和校验和。 */
  for (index = 0U; index < 40U; index++)
  {
    if (!Dht11_WaitFor(GPIO_PIN_SET, 70U))
    {
      return false;
    }

    /* 高电平持续时间较长表示 1，较短表示 0。 */
    Dht11_DelayUs(40U);
    bit = (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET) ? 1U : 0U;
    bytes[index / 8U] = (uint8_t)((bytes[index / 8U] << 1U) | bit);

    if (!Dht11_WaitFor(GPIO_PIN_RESET, 100U))
    {
      return false;
    }
  }

  /* 前四个字节相加的低八位必须等于第五个字节。 */
  if ((uint8_t)(bytes[0] + bytes[1] + bytes[2] + bytes[3]) != bytes[4])
  {
    return false;
  }

  /* 使用扩大十倍的整数保存一位小数，避免在 MCU 上引入不必要的浮点计算。 */
  data->humidity_tenth = (uint16_t)(bytes[0] * 10U + bytes[1]);
  data->temperature_tenth = (int16_t)(bytes[2] * 10U + bytes[3]);
  return true;
}
