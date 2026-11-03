/*
 * 华为云 IoTDA $ota 事件的受限解析与上报 JSON 构造接口。
 * 解析器只支持本项目用到的固定字段，不是通用 JSON 实现；输入缓冲区所有权
 * 始终属于调用者，错误诊断不会复制 URL、令牌或其他敏感值。
 */
#ifndef OTA_CLOUD_H
#define OTA_CLOUD_H

#include <stdbool.h>
#include <stdint.h>
#include "ota_layout.h"

#define OTA_CLOUD_VERSION_MAX_LENGTH       31U
/* OBS 签名下载 URL 会带较长的鉴权参数，不能按普通 HTTP 地址预留。 */
#define OTA_CLOUD_URL_MAX_LENGTH           4095U
#define OTA_CLOUD_ACCESS_TOKEN_MAX_LENGTH  511U

typedef enum
{
    OTA_CLOUD_EVENT_NONE = 0,       /* 尚未解析到本项目支持的事件。 */
    OTA_CLOUD_EVENT_VERSION_QUERY,  /* 云端询问设备当前固件版本。 */
    OTA_CLOUD_EVENT_UPGRADE         /* 云端要求下载并安装一份新固件。 */
} OtaCloudEventKind_t;

/* 一条解析成功的 IoTDA $ota 下行事件。 */
typedef struct
{
    OtaCloudEventKind_t kind; /* 决定调用者走“回报版本”还是“开始升级”分支。 */
    OtaSlot_t target_slot;    /* 从 custom_info 的 slot=A/B 解析而来。 */
    uint32_t file_size;       /* 云端声明的固件字节数，不能超过一个槽位。 */
    bool need_authorization; /* IoTDA 存储包必须附加 Bearer 临时令牌；OBS 包不需要。 */
    char version[OTA_CLOUD_VERSION_MAX_LENGTH + 1U]; /* 新固件版本字符串。 */
    char url[OTA_CLOUD_URL_MAX_LENGTH + 1U];         /* HTTP/HTTPS 下载地址。 */
    char sha256[65];          /* 64 位十六进制摘要，解析后统一为小写。 */
    char access_token[OTA_CLOUD_ACCESS_TOKEN_MAX_LENGTH + 1U]; /* 临时令牌。 */
} OtaCloudEvent_t;

/*
 * 解析 version_query、firmware_upgrade 或 firmware_upgrade_v2。
 * json/event 不能为 NULL；成功时完整填充 event，字段缺失或越界时返回 false。
 */
bool OtaCloud_ParseEvent(const char *json, OtaCloudEvent_t *event);

/* 返回最近一次解析失败字段的静态字符串；调用者不得修改或释放。 */
const char *OtaCloud_GetLastParseError(void);

/* 生成 $ota/version_report 上行事件的 JSON。 */
bool OtaCloud_BuildVersionReport(char *json, uint32_t json_size,
                                 const char *version);

/* 生成 $ota/upgrade_progress_report 上行事件的 JSON。 */
bool OtaCloud_BuildProgressReport(char *json, uint32_t json_size,
                                  const char *version, uint8_t progress,
                                  int32_t result_code);

#endif /* OTA_CLOUD_H */
