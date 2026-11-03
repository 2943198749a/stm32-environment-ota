/*
 * 华为云 IoTDA $ota JSON 的无动态内存实现。
 *
 * 解析入口先识别 service_id/event_type，再逐项读取版本、URL、长度、摘要和
 * 目标槽位；任何字段缺失或越界都整条拒绝。这里使用受限字符串扫描而非完整
 * JSON 库，是为了减小 STM32 固件体积，所以只能解析本项目约定的简单消息。
 */
#include <stdio.h>
#include <string.h>
#include "ota_cloud.h"
#include "ota_integrity.h"

/* 保存最近一次 OTA 通知解析失败的原因，方便定位云端字段。 */
static const char *g_ota_cloud_parse_error = "unknown";

const char *OtaCloud_GetLastParseError(void)
{
    return g_ota_cloud_parse_error;
}

/* 在 JSON 文本中寻找某个固定键对应的值起始位置。 */
static const char *OtaCloud_FindValue(const char *json, const char *key)
{
    char pattern[40];
    const char *position;

    if ((json == NULL) || (key == NULL))
    {
        return NULL;
    }

    (void)snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    position = strstr(json, pattern);
    if (position == NULL)
    {
        return NULL;
    }

    position += strlen(pattern);
    while ((*position == ' ') || (*position == '\t') || (*position == '\r') ||
           (*position == '\n'))
    {
        position++;
    }
    if (*position != ':')
    {
        return NULL;
    }
    position++;
    while ((*position == ' ') || (*position == '\t') || (*position == '\r') ||
           (*position == '\n'))
    {
        position++;
    }
    return position;
}

/* 读取 JSON 中一个不包含转义引号的字符串字段。IoTDA OTA 字段满足这个条件。 */
static bool OtaCloud_ReadString(const char *json, const char *key,
                                char *output, uint32_t output_size)
{
    const char *start = OtaCloud_FindValue(json, key);
    const char *end;
    uint32_t length;

    if ((start == NULL) || (output == NULL) || (output_size == 0U) ||
        (*start != '\"'))
    {
        return false;
    }

    start++;
    end = strchr(start, '\"');
    if (end == NULL)
    {
        return false;
    }

    length = (uint32_t)(end - start);
    if ((length == 0U) || (length >= output_size))
    {
        return false;
    }

    memcpy(output, start, length);
    output[length] = '\0';
    return true;
}

/* 读取 JSON 中的无符号十进制整数，拒绝负数、空值和溢出。 */
static bool OtaCloud_ReadUint32(const char *json, const char *key,
                                uint32_t *value)
{
    const char *position = OtaCloud_FindValue(json, key);
    uint32_t result = 0U;
    uint8_t digit_count = 0U;
    uint8_t digit;

    if ((position == NULL) || (value == NULL))
    {
        return false;
    }

    while ((*position >= '0') && (*position <= '9'))
    {
        digit = (uint8_t)(*position - '0');
        if (result > ((0xFFFFFFFFUL - digit) / 10U))
        {
            return false;
        }
        result = result * 10U + digit;
        position++;
        digit_count++;
    }

    if (digit_count == 0U)
    {
        return false;
    }
    *value = result;
    return true;
}

/* 检查版本字符串，避免其被直接拼进 JSON 时破坏上行 JSON 格式。 */
static bool OtaCloud_VersionIsSafe(const char *version)
{
    uint32_t index;
    char value;

    if ((version == NULL) || (version[0] == '\0'))
    {
        return false;
    }

    for (index = 0U; version[index] != '\0'; index++)
    {
        value = version[index];
        if (!(((value >= '0') && (value <= '9')) ||
              ((value >= 'A') && (value <= 'Z')) ||
              ((value >= 'a') && (value <= 'z')) ||
              (value == '.') || (value == '-') || (value == '_')))
        {
            return false;
        }
    }
    return true;
}

/* 检查云端 sign 是否为 64 位十六进制 SHA-256（大小写均可）。 */
static bool OtaCloud_Sha256IsValid(const char hash[65])
{
    uint32_t index;
    char value;

    if (hash == NULL)
    {
        return false;
    }

    for (index = 0U; index < 64U; index++)
    {
        value = hash[index];
        if (!(((value >= '0') && (value <= '9')) ||
              ((value >= 'a') && (value <= 'f')) ||
              ((value >= 'A') && (value <= 'F'))))
        {
            return false;
        }
    }
    return hash[64] == '\0';
}

/*
 * 将 SHA256 中的大写 A-F 转成小写。
 * 这样无论云端使用大写还是小写，后续固件校验时都只处理一种格式。
 */
static void OtaCloud_NormalizeSha256(char hash[65])
{
    uint32_t index;

    for (index = 0U; index < 64U; index++)
    {
        if ((hash[index] >= 'A') && (hash[index] <= 'F'))
        {
            hash[index] = (char)(hash[index] - 'A' + 'a');
        }
    }
}

/* 将 custom_info 中的 slot=A/B 转成内部槽位枚举。 */
static OtaSlot_t OtaCloud_ReadSlot(const char *custom_info)
{
    if (custom_info == NULL)
    {
        return OTA_SLOT_NONE;
    }
    if (strstr(custom_info, "slot=A") != NULL)
    {
        return OTA_SLOT_A;
    }
    if (strstr(custom_info, "slot=B") != NULL)
    {
        return OTA_SLOT_B;
    }
    return OTA_SLOT_NONE;
}

bool OtaCloud_ParseEvent(const char *json, OtaCloudEvent_t *event)
{
    char service_id[16];
    char event_type[32];
    char custom_info[32];

    g_ota_cloud_parse_error = "none";

    if ((json == NULL) || (event == NULL))
    {
        g_ota_cloud_parse_error = "argument";
        return false;
    }

    /* 先清零输出，确保失败时调用者不会误用上一次事件残留的数据。 */
    memset(event, 0, sizeof(OtaCloudEvent_t));
    if ((!OtaCloud_ReadString(json, "service_id", service_id, sizeof(service_id))) ||
        (strcmp(service_id, "$ota") != 0) ||
        (!OtaCloud_ReadString(json, "event_type", event_type, sizeof(event_type))))
    {
        g_ota_cloud_parse_error = "service or event type";
        return false;
    }

    if (strcmp(event_type, "version_query") == 0)
    {
        event->kind = OTA_CLOUD_EVENT_VERSION_QUERY;
        return true;
    }

    if ((strcmp(event_type, "firmware_upgrade") != 0) &&
        (strcmp(event_type, "firmware_upgrade_v2") != 0))
    {
        g_ota_cloud_parse_error = "unsupported event type";
        return false;
    }

    if ((!OtaCloud_ReadString(json, "version", event->version,
                              sizeof(event->version))) ||
        (!OtaCloud_VersionIsSafe(event->version)))
    {
        g_ota_cloud_parse_error = "version";
        return false;
    }
    if ((!OtaCloud_ReadString(json, "url", event->url, sizeof(event->url))) ||
        (strncmp(event->url, "http", 4U) != 0))
    {
        g_ota_cloud_parse_error = "URL";
        return false;
    }
    if ((!OtaCloud_ReadUint32(json, "file_size", &event->file_size)) ||
        (!Ota_ImageSizeIsValid(event->file_size)))
    {
        g_ota_cloud_parse_error = "file size";
        return false;
    }
    if ((!OtaCloud_ReadString(json, "sign", event->sha256,
                              sizeof(event->sha256))) ||
        (!OtaCloud_Sha256IsValid(event->sha256)))
    {
        g_ota_cloud_parse_error = "SHA256 sign";
        return false;
    }
    if (!OtaCloud_ReadString(json, "custom_info", custom_info,
                             sizeof(custom_info)))
    {
        g_ota_cloud_parse_error = "custom info";
        return false;
    }

    /* 云端签名格式合法后，统一保存为小写 SHA256。 */
    OtaCloud_NormalizeSha256(event->sha256);

    event->target_slot = OtaCloud_ReadSlot(custom_info);
    if (event->target_slot == OTA_SLOT_NONE)
    {
        g_ota_cloud_parse_error = "slot";
        return false;
    }

    /*
     * firmware_upgrade 表示升级包存储在 IoTDA，必须使用本次通知携带的
     * 临时 access_token。若令牌缺失，绝不能先擦除 B 槽再发起必然失败的下载。
     * firmware_upgrade_v2 表示 OBS 签名 URL，文档要求不附加 Authorization 头。
     */
    if (strcmp(event_type, "firmware_upgrade") == 0)
    {
        if (!OtaCloud_ReadString(json, "access_token", event->access_token,
                                 sizeof(event->access_token)))
        {
            g_ota_cloud_parse_error = "access token";
            return false;
        }
        event->need_authorization = true;
    }
    else
    {
        event->need_authorization = false;
    }

    event->kind = OTA_CLOUD_EVENT_UPGRADE;
    return true;
}

bool OtaCloud_BuildVersionReport(char *json, uint32_t json_size,
                                 const char *version)
{
    int length;

    if ((json == NULL) || (json_size == 0U) || (!OtaCloud_VersionIsSafe(version)))
    {
        return false;
    }

    length = snprintf(json, json_size,
                      "{\"services\":[{\"service_id\":\"$ota\","
                      "\"event_type\":\"version_report\","
                      /*
                       * 华为云 OTA 的标准示例同时上报软件和固件版本。
                       * 本项目只升级 STM32 固件，两者使用同一个版本号即可。
                       */
                      "\"paras\":{\"sw_version\":\"%s\","
                      "\"fw_version\":\"%s\"}}]}", version, version);
    return (length > 0) && ((uint32_t)length < json_size);
}

bool OtaCloud_BuildProgressReport(char *json, uint32_t json_size,
                                  const char *version, uint8_t progress,
                                  int32_t result_code)
{
    int length;

    if ((json == NULL) || (json_size == 0U) || (!OtaCloud_VersionIsSafe(version)) ||
        (progress > 100U) || (result_code < 0) || (result_code > 255))
    {
        return false;
    }

    length = snprintf(json, json_size,
                      "{\"services\":[{\"service_id\":\"$ota\","
                      "\"event_type\":\"upgrade_progress_report\","
                      "\"paras\":{\"version\":\"%s\",\"progress\":%u,"
                      "\"result_code\":%ld}}]}",
                      version, (unsigned int)progress, (long)result_code);
    return (length > 0) && ((uint32_t)length < json_size);
}
