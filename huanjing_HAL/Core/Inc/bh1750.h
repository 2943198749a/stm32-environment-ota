/* BH1750 光照传感器接口。 */
#ifndef __BH1750_H__
#define __BH1750_H__

#include <stdbool.h>
#include <stdint.h>

/*
 * 通过 I2C1 让 BH1750 上电并进入连续高分辨率测量模式。
 * 应在传感器任务中调用一次；总线应答正常返回 true。
 */
bool Bh1750_Init(void);

/*
 * 读取两个测量字节并换算为 lux；lux 不能为 NULL。
 * I2C 超时或设备未应答时返回 false，调用者不得使用旧输出冒充新数据。
 */
bool Bh1750_ReadLux(uint16_t *lux);

#endif
