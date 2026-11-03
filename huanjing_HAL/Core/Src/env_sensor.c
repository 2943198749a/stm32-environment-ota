/*
 * 环境数据汇总：本文件不处理硬件时序，只把三个传感器的结果集中起来。
 * 传感器任务每秒调用 Update，随后把 GetData 返回的快照复制到任务共享区。
 * 各驱动互相独立：一个传感器失败只清自己的 valid 标志，不影响其他数据。
 */
#include "env_sensor.h"
#include "bh1750.h"
#include "dht11.h"
#include "mq135.h"

#include <string.h>

/* 最新一帧环境数据，由传感器任务更新，再交给 FreeRTOS 共享副本。 */
static EnvData_t g_data;

void EnvSensor_Init(void)
{
  /* 开机先清空所有数据和有效标志，LCD 会显示 NO DATA。 */
  (void)memset(&g_data, 0, sizeof(g_data));

  /* DHT11 需要准备微秒计时器；BH1750 初始化失败时下次读取仍会显示无数据。 */
  Dht11_Init();
  (void)Bh1750_Init();
}

void EnvSensor_Update(void)
{
  Dht11Data_t dht_data;
  Mq135Data_t mq135_data;
  uint16_t lux;

  /* 温湿度来自同一次 DHT11 读取，因此两个有效标志一起更新。 */
  if (Dht11_Read(&dht_data))
  {
    g_data.temperature_tenth = dht_data.temperature_tenth;
    g_data.humidity_tenth = dht_data.humidity_tenth;
    g_data.temperature_valid = true;
    g_data.humidity_valid = true;
  }
  else
  {
    g_data.temperature_valid = false;
    g_data.humidity_valid = false;
  }

  /* BH1750 通过 I2C 返回光照；它的失败不应抹掉温湿度读取结果。 */
  if (Bh1750_ReadLux(&lux))
  {
    g_data.light_lux = lux;
    g_data.light_valid = true;
  }
  else
  {
    g_data.light_valid = false;
  }

  /* MQ135 当前数值是未标定的趋势估算，只用于界面展示。 */
  if (Mq135_Read(&mq135_data))
  {
    g_data.air_ppm = mq135_data.estimated_ppm;
    g_data.air_valid = true;
  }
  else
  {
    g_data.air_valid = false;
  }
}

const EnvData_t *EnvSensor_GetData(void)
{
  /* 返回内部最新数据；调用者应复制它，而不是在多个任务间直接共享指针。 */
  return &g_data;
}
