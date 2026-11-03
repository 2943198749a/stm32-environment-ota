/* 应用侧 OTA 下载写入、镜像确认和复位接口。 */
#ifndef OTA_APP_H
#define OTA_APP_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32f1xx_hal.h"
#include "ota_layout.h"

/* Flash 写入接口每次允许的最大字节数；当前 UART 协议分块上限另见 ota_uart.h。 */
#define OTA_APP_WRITE_MAX_BYTES  1024U

/* 应用启动时调用一次，清空本次 OTA 的运行状态。 */
void OtaApp_Init(void);

/* 判断本次下载是否只会写入“另一”应用槽位，避免覆盖正在运行的程序。 */
bool OtaApp_TargetSlotIsAllowed(OtaSlot_t running_slot,
                                OtaSlot_t target_slot,
                                uint32_t image_size);

/* 擦除目标槽位所需页面，并记录本次下载的长度和 SHA-256 期望值。 */
HAL_StatusTypeDef OtaApp_Begin(OtaSlot_t running_slot,
                               OtaSlot_t target_slot,
                               uint32_t image_size,
                               const char expected_sha256[65]);

/* 将一块连续的二进制数据写入目标槽位，offset 必须等于已写入长度。 */
HAL_StatusTypeDef OtaApp_Write(uint32_t offset,
                               const uint8_t *data,
                               uint32_t length);

/* 检查长度、SHA-256 和启动入口，计算 CRC32 存入待启动记录；不在此处重启。 */
HAL_StatusTypeDef OtaApp_Finalize(void);

/* 取消本次下载；不会擦写正在运行的槽位。 */
void OtaApp_Abort(void);

/* 稳定运行后确认当前槽位，确认后它才会成为新的 active 槽位。 */
HAL_StatusTypeDef OtaApp_ConfirmRunning(OtaSlot_t running_slot);

/*
 * 当前槽位是否正等待“试运行成功”的确认。
 * 只有 Bootloader 刚刚从 pending 槽位启动新固件时才返回 true，
 * 旧的 active 槽位不能把自己误报成一次 OTA 成功。
 */
bool OtaApp_IsConfirmationRequired(OtaSlot_t running_slot);

/* 查询是否正在写入 OTA 固件。 */
bool OtaApp_IsBusy(void);

#endif /* OTA_APP_H */
