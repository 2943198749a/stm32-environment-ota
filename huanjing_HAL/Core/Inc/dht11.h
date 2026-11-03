/* DHT11 温湿度传感器接口。 */
#ifndef __DHT11_H__
#define __DHT11_H__

#include <stdbool.h>
#include <stdint.h>

/* DHT11 一次读取到的温湿度，单位均为 0.1。 */
typedef struct
{
  int16_t temperature_tenth; /* 温度，例如 253 表示 25.3 摄氏度。 */
  uint16_t humidity_tenth;   /* 湿度，例如 605 表示 60.5%。 */
} Dht11Data_t;

/*
 * 初始化 DHT11 使用的微秒计时和 GPIO 空闲状态；在传感器任务启动时调用一次。
 * 初始化及读取包含硬件时序操作，不能在中断服务函数中调用。
 */
void Dht11_Init(void);

/*
 * 读取一帧 40 位数据并校验；data 不能为 NULL。
 * 成功时写入 0.1℃/0.1% 单位的数据并返回 true，超时或校验失败返回 false。
 */
bool Dht11_Read(Dht11Data_t *data);

#endif
