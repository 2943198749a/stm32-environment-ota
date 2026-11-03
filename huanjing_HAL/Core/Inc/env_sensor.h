/* 环境传感器汇总接口：把温湿度、光照和空气质量统一交给应用层。 */
#ifndef __ENV_SENSOR_H
#define __ENV_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

/* 一帧环境数据；每个 valid 标志表示同名数据本次是否读取成功。 */
typedef struct
{
  int16_t temperature_tenth; /* 温度，单位 0.1 摄氏度。 */
  uint16_t humidity_tenth;   /* 湿度，单位 0.1%。 */
  uint16_t light_lux;        /* 光照，单位 lux。 */
  uint16_t air_ppm;          /* 空气质量显示用估算值。 */
  /*
   * 数值与有效标志分开保存：读取失败时保留上次数值便于调试，但界面和上传
   * 必须先看 valid，不能把旧值当成当前测量结果。
   */
  bool temperature_valid; /* 本次 DHT11 温度读取是否成功。 */
  bool humidity_valid;    /* 本次 DHT11 湿度读取是否成功。 */
  bool light_valid;       /* 本次 BH1750 光照读取是否成功。 */
  bool air_valid;         /* 本次 MQ135 ADC 读取是否成功。 */
} EnvData_t;

/*
 * 初始化 DHT11 和 BH1750；应在 FreeRTOS 传感器任务启动后调用一次。
 * I2C 设备未接线时可能等待 HAL 超时，因此不能在调度器启动前调用。
 */
void EnvSensor_Init(void);

/*
 * 顺序读取全部传感器并更新内部快照；失败的项目只清除对应 valid 标志。
 * 函数可能阻塞于硬件超时，只能由传感器任务调用。
 */
void EnvSensor_Update(void);

/*
 * 返回模块内部最新快照的只读指针，永不返回 NULL。
 * 指针所有权仍属于模块；调用者应立即复制，不要跨任务长期保存。
 */
const EnvData_t *EnvSensor_GetData(void);

#endif
