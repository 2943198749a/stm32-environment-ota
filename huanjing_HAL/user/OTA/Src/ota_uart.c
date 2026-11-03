/*
 * ESP32 Arduino 网关到 STM32 的 OTA 串口接收实现。
 *
 * 阅读路线（先看下面这几个函数，不必从 CRC 算法开始）：
 *   Process：从串口缓冲区取字节，分给文本处理或数据帧处理。
 *   HandleControlLine：识别 BEGIN / END / ABORT 三种升级命令。
 *   HandleBegin：检查开始命令，调用 OtaApp_Begin 准备接收。
 *   AcknowledgeFrame：一块数据收齐且 CRC 正确后，调用 OtaApp_Write。
 *   收到 END 时调用 OtaApp_Finalize；成功后回复 OTA_OK，再复位。
 *
 * 文本行负责开始、结束和中止升级；固件数据帧格式为：
 * [A5 5A][sequence LE16][length LE16][payload][CRC16 LE16]。
 * CRC 覆盖 sequence、length 和 payload，不包含同步字节及帧末保存的 CRC。
 * UART 中断只缓存字节，本状态机和 Flash 写入全部在 OTA 任务上下文执行。
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "ota_uart.h"

/* 电脑单元测试不需要 HAL、串口或 Flash，只验证协议基础函数。 */
#ifndef OTA_UART_HOST_TEST
#include "ota_app.h"
#include "ota_integrity.h"
#include "ota_port.h"
#endif

/* OTA_BEGIN 命令最长约 120 字节，留出余量以拒绝过长输入。 */
#define OTA_UART_LINE_MAX            160U

typedef enum
{
    OTA_UART_WAIT_SYNC_1 = 0,     /* 等待帧头第一个字节 0xA5。 */
    OTA_UART_WAIT_SYNC_2,         /* 已收到 0xA5，继续等待 0x5A。 */
    OTA_UART_READ_SEQUENCE_LOW,   /* 序号低字节。 */
    OTA_UART_READ_SEQUENCE_HIGH,  /* 序号高字节。 */
    OTA_UART_READ_LENGTH_LOW,     /* 负载长度低字节。 */
    OTA_UART_READ_LENGTH_HIGH,    /* 负载长度高字节。 */
    OTA_UART_READ_PAYLOAD,        /* 按 received_length 收集固件数据。 */
    OTA_UART_READ_CRC_LOW,        /* 帧尾 CRC16 低字节。 */
    OTA_UART_READ_CRC_HIGH        /* CRC 收齐后校验并决定 ACK/NACK。 */
} OtaUartFrameState_t;

#ifndef OTA_UART_HOST_TEST
/* 当前数据帧的解析状态，与整次下载的进度分开管理。 */
typedef struct
{
    OtaUartFrameState_t state;          /* 下一字节应按哪个字段解析。 */
    uint16_t sequence;                 /* 本帧序号。 */
    uint16_t length;                   /* 本帧声明的负载长度。 */
    uint16_t crc;                      /* 帧尾收到的 CRC16。 */
    uint16_t index;                    /* 负载下一写入位置。 */
    uint8_t payload[OTA_UART_MAX_PAYLOAD];
} OtaUartFrame_t;

/* 串口 OTA 会话上下文，仅由 OTA 任务更新，不在中断中解析。 */
typedef struct
{
    OtaSlot_t running_slot;            /* 当前运行槽位。 */
    bool receiving_firmware;          /* 已接受 BEGIN，正在接收固件。 */
    uint16_t next_sequence;            /* 下一帧期望序号。 */
    uint32_t written_bytes;            /* 已交给 Flash 模块写好的长度。 */
    OtaUartFrame_t frame;              /* 当前二进制帧。 */
    char control_line[OTA_UART_LINE_MAX]; /* 当前文本命令。 */
    uint16_t line_length;              /* 已缓存的文本长度，不含结束符。 */
    bool reading_control_line;        /* 帧间正在接收文本命令。 */
    bool reset_pending;               /* 应答发送后等待软件复位。 */
    char status_message[32];          /* 供界面读取的状态短消息。 */
} OtaUartContext_t;

static OtaUartContext_t uart;

/* 辅助函数声明：这里只告诉编译器函数名称，具体实现放在主流程后。 */
static void OtaUart_WriteLe16(uint8_t output[2], uint16_t value);
static void OtaUart_SendLine(const char *text);
static void OtaUart_SendSequenceResponse(const char *name, uint16_t sequence);
static void OtaUart_SetMessage(const char *text);
static void OtaUart_ResetFrame(void);
static bool OtaUart_VersionIsSafe(const char *text);
static bool OtaUart_NormalizeSha256(char text[65]);
static void OtaUart_HandleBegin(const char *line);
static void OtaUart_HandleControlLine(void);
static void OtaUart_PushLineByte(uint8_t byte);
static void OtaUart_AcknowledgeFrame(void);
static void OtaUart_PushFrameByte(uint8_t byte);

/**
 * @brief 初始化串口 OTA 会话并设置当前运行槽位。
 * running_slot 是当前应用槽；无返回值，所有计数、缓冲区及复位标志统一清零。
 * 帧解析回到等待同步字状态，状态消息设为 ready；只清 UART 上下文，不中止 OtaApp。
 */
void OtaUart_Init(OtaSlot_t running_slot)
{
    (void)memset(&uart, 0, sizeof(uart));
    uart.running_slot = running_slot;
    OtaUart_ResetFrame();
    OtaUart_SetMessage("OTA gateway ready");
}

/**
 * @brief 取出当前串口缓冲数据，按会话状态分发给文本或二进制帧解析器。
 * 无参数、无返回值；在 OTA 任务中反复调用，读不到数据时结束本轮。
 * 解析可能触发 Flash 擦写和应答；下载完成的 OTA_OK 发出后，在本轮尾部执行复位。
 */
void OtaUart_Process(void)
{
    uint8_t byte;

    while (OtaPort_ReadByte(&byte))
    {
        if (uart.reading_control_line || !uart.receiving_firmware)
        {
            OtaUart_PushLineByte(byte);
        }
        else
        {
            OtaUart_PushFrameByte(byte);
        }
    }

    /*
     * 新镜像已写入且校验通过：先把 OTA_OK 交给 UART，再在本次处理的尾部复位。
     * 重启后由 Bootloader 选择 pending 槽位；不能在发送应答前立即复位。
     */
    if (uart.reset_pending)
    {
        uart.reset_pending = false;
        OtaPort_ResetToBootloader();
    }
}

/**
 * @brief 根据已收齐的文本命令执行升级开始、结束、中止或网关状态处理。
 * 无参数；读取 uart.control_line，要求调用前补好字符串结束符；无返回值。
 * 空闲时识别 BEGIN/STATUS；接收中识别 END/ABORT，未知文本忽略。
 * END 校验成功应答 OTA_OK 并延迟复位；校验失败或中止会清除接收状态并发送错误应答。
 */
static void OtaUart_HandleControlLine(void)
{
    if (!uart.receiving_firmware)
    {
        if (strncmp(uart.control_line, "OTA_BEGIN,", 10U) == 0)
        {
            OtaUart_HandleBegin(uart.control_line);
        }
        else if (strncmp(uart.control_line, "STATUS,", 7U) == 0)
        {
            OtaPort_HandleGatewayStatus(uart.control_line);
        }
        return;
    }

    /* ESP32 说“发完了”，STM32 仍要自己检查总长度和整包摘要。 */
    if (strcmp(uart.control_line, "OTA_END") == 0)
    {
        if (OtaApp_Finalize() == HAL_OK)
        {
            uart.receiving_firmware = false;
            OtaUart_SetMessage("OTA ready reboot");
            OtaUart_SendLine("OTA_OK");
            uart.reset_pending = true;
        }
        else
        {
            OtaApp_Abort();
            uart.receiving_firmware = false;
            OtaUart_SetMessage("OTA verify error");
            OtaUart_SendLine("OTA_ERROR,verify");
        }
        OtaUart_ResetFrame();
    }
    else if (strcmp(uart.control_line, "OTA_ABORT") == 0)
    {
        OtaApp_Abort();
        uart.receiving_firmware = false;
        OtaUart_ResetFrame();
        OtaUart_SetMessage("OTA aborted");
        OtaUart_SendLine("OTA_ERROR,abort");
    }
}

/**
 * @brief 解析 OTA_BEGIN 的版本、目标槽、固件长度和 SHA-256，并启动下载。
 * line 为以结束符结尾的命令文本；检查版本字符、非零偶数长度、槽容量和摘要格式。
 * 无返回值；参数错误回复 OTA_ERROR,begin，应用层拒绝回复 OTA_ERROR,slot。
 * 成功擦除并开始后清零帧序号和进度，切换到固件接收状态并回复 OTA_READY。
 */
static void OtaUart_HandleBegin(const char *line)
{
    char version[32];
    char sha256[65];
    char slot_text[2] = {0};
    unsigned long file_size;
    OtaSlot_t target_slot;

    /* || 从左到右检查，解析失败就停止，避免使用未赋值的参数。
     * 版本字符有效、长度非零且为偶数并能放入分区、摘要为十六进制。 */
    if ((sscanf(line, "OTA_BEGIN,%31[^,],%1[A-B],%lu,%64s", version,
                slot_text, &file_size, sha256) != 4) ||
        !OtaUart_VersionIsSafe(version) || (file_size == 0UL) ||
        ((file_size & 1UL) != 0UL) || (file_size > OTA_SLOT_SIZE) ||
        !OtaUart_NormalizeSha256(sha256))
    {
        OtaUart_SetMessage("OTA begin error");
        OtaUart_SendLine("OTA_ERROR,begin");
        return;
    }

    target_slot = (slot_text[0] == 'A') ? OTA_SLOT_A : OTA_SLOT_B;
    if (OtaApp_Begin(uart.running_slot, target_slot, (uint32_t)file_size,
                     sha256) != HAL_OK)
    {
        OtaUart_SetMessage("OTA slot error");
        OtaUart_SendLine("OTA_ERROR,slot");
        return;
    }

    uart.receiving_firmware = true;
    uart.next_sequence = 0U;
    uart.written_bytes = 0U;
    uart.reading_control_line = false;
    OtaUart_ResetFrame();
    OtaUart_SetMessage("OTA receiving");
    OtaUart_SendLine("OTA_READY");
}

/**
 * @brief 将已经通过帧 CRC 校验的负载写入目标固件槽，并回复序号应答。
 * 无参数；使用当前帧的序号、长度和负载，要求调用方已校验帧长度及 CRC。
 * 序号不匹配或写入失败回复期望序号的 NACK；写入成功才累计进度并递增序号。
 * 无返回值；可能写 Flash，同一会话必须串行调用。
 */
static void OtaUart_AcknowledgeFrame(void)
{
    /* 序号错误时 || 会跳过 Write；写入失败也统一回复 NACK。 */
    if ((uart.frame.sequence != uart.next_sequence) ||
        (OtaApp_Write(uart.written_bytes, uart.frame.payload,
                      uart.frame.length) != HAL_OK))
    {
        OtaUart_SendSequenceResponse("OTA_NACK", uart.next_sequence);
        return;
    }

    uart.written_bytes += uart.frame.length;
    OtaUart_SendSequenceResponse("OTA_ACK", uart.next_sequence);
    uart.next_sequence++;
}

/**
 * @brief 把一个串口字节追加到文本命令缓冲区。
 * byte 为本次收到的字节；忽略回车，遇换行补结束符并分发完整命令。
 * 无返回值；缓冲满时清除本行接收位置及文本模式，并记录行过长状态。
 */
static void OtaUart_PushLineByte(uint8_t byte)
{
    if (byte == '\n')
    {
        uart.control_line[uart.line_length] = '\0';
        OtaUart_HandleControlLine();
        uart.line_length = 0U;
        uart.reading_control_line = false;
        return;
    }
    if (byte == '\r')
    {
        return;
    }
    if (uart.line_length < (OTA_UART_LINE_MAX - 1U))
    {
        uart.control_line[uart.line_length++] = (char)byte;
    }
    else
    {
        uart.line_length = 0U;
        uart.reading_control_line = false;
        OtaUart_SetMessage("OTA line too long");
    }
}

/**
 * @brief 按同步头、序号、长度、负载和 CRC16 的状态顺序解析一个字节。
 * byte 为串口输入；序号、长度及 CRC 均按小端格式接收；无返回值。
 * 非法长度重置帧；帧间遇 O 转文本解析；完整帧的 CRC 覆盖序号、长度和负载。
 * CRC 正确后处理写入及 ACK，错误回复 NACK；处理完帧回到等待同步头。
 */
static void OtaUart_PushFrameByte(uint8_t byte)
{

    switch (uart.frame.state)
    {
        case OTA_UART_WAIT_SYNC_1:
            if (byte == OTA_UART_SYNC_1)
            {
                uart.frame.state = OTA_UART_WAIT_SYNC_2;
            }
            else if (byte == 'O')
            {
                /* 二进制帧之间以 O 开头的内容只能是 OTA_END 或 OTA_ABORT。 */
                uart.reading_control_line = true;
                uart.line_length = 0U;
                OtaUart_PushLineByte(byte);
            }
            break;

        case OTA_UART_WAIT_SYNC_2:
            uart.frame.state = (byte == OTA_UART_SYNC_2) ?
                                      OTA_UART_READ_SEQUENCE_LOW : OTA_UART_WAIT_SYNC_1;
            break;

        case OTA_UART_READ_SEQUENCE_LOW:
            uart.frame.sequence = byte;
            uart.frame.state = OTA_UART_READ_SEQUENCE_HIGH;
            break;

        case OTA_UART_READ_SEQUENCE_HIGH:
            uart.frame.sequence |= (uint16_t)byte << 8U;
            uart.frame.state = OTA_UART_READ_LENGTH_LOW;
            break;

        case OTA_UART_READ_LENGTH_LOW:
            uart.frame.length = byte;
            uart.frame.state = OTA_UART_READ_LENGTH_HIGH;
            break;

        case OTA_UART_READ_LENGTH_HIGH:
            uart.frame.length |= (uint16_t)byte << 8U;
            if ((uart.frame.length == 0U) ||
                (uart.frame.length > OTA_UART_MAX_PAYLOAD) ||
                ((uart.frame.length & 1U) != 0U))
            {
                OtaUart_ResetFrame();
            }
            else
            {
                uart.frame.index = 0U;
                uart.frame.state = OTA_UART_READ_PAYLOAD;
            }
            break;

        case OTA_UART_READ_PAYLOAD:
            uart.frame.payload[uart.frame.index++] = byte;
            if (uart.frame.index >= uart.frame.length)
            {
                uart.frame.state = OTA_UART_READ_CRC_LOW;
            }
            break;

        case OTA_UART_READ_CRC_LOW:
            uart.frame.crc = byte;
            uart.frame.state = OTA_UART_READ_CRC_HIGH;
            break;

        case OTA_UART_READ_CRC_HIGH:
            uart.frame.crc |= (uint16_t)byte << 8U;
            {
                uint8_t crc_data[4U + OTA_UART_MAX_PAYLOAD];
                uint16_t crc;
                OtaUart_WriteLe16(&crc_data[0], uart.frame.sequence);
                OtaUart_WriteLe16(&crc_data[2], uart.frame.length);
                /* OtaUart_Crc16 不支持增量，因此把本帧放入临时数组计算。 */
                memcpy(&crc_data[4U], uart.frame.payload,
                       uart.frame.length);
                crc = OtaUart_Crc16(crc_data,
                                    (uint32_t)uart.frame.length + 4U);
                if (uart.frame.crc == crc)
                {
                    OtaUart_AcknowledgeFrame();
                }
                else
                {
                    OtaUart_SendSequenceResponse("OTA_NACK",
                                                 uart.next_sequence);
                }
            }
            OtaUart_ResetFrame();
            break;

        default:
            OtaUart_ResetFrame();
            break;
    }
}

/**
 * @brief 查询串口层是否正在接收固件。
 * 无参数；返回 true 表示已接受 BEGIN 且还没有完成或中止会话。
 * 只读取会话标志，普通数据上报任务可据此暂停发送，避免占用升级链路。
 */
bool OtaUart_IsBusy(void)
{
    return uart.receiving_firmware;
}

/**
 * @brief 获取当前串口 OTA 状态短消息。
 * 无参数；返回内部缓冲区的只读指针，不需要释放，调用者不得修改内容。
 * 后续状态变化或重新初始化会覆盖该缓冲区；需要保留历史消息时应自行复制。
 */
const char *OtaUart_GetLastMessage(void)
{
    return uart.status_message;
}

/**
 * @brief 将当前帧解析位置重置到等待第一个同步字节。
 * 无参数、无返回值；只重置状态、负载索引和声明长度。
 * 保留会话进度及负载内容；旧序号和 CRC 会在下一帧接收时被覆盖。
 */
static void OtaUart_ResetFrame(void)
{
    uart.frame.state = OTA_UART_WAIT_SYNC_1;
    uart.frame.index = 0U;
    uart.frame.length = 0U;
}

/**
 * @brief 格式化并发送一行 OTA 应答，末尾追加 CRLF。
 * text 为不含行结束符的应答文本；空指针直接忽略；无返回值。
 * 使用固定大小栈缓冲区，格式化失败或截断时不发送；发送结果由适配层处理。
 */
static void OtaUart_SendLine(const char *text)
{
    char line[48];
    int length;

    if (text == NULL)
    {
        return;
    }
    length = snprintf(line, sizeof(line), "%s\r\n", text);
    if ((length > 0) && ((uint32_t)length < sizeof(line)))
    {
        OtaPort_Send((const uint8_t *)line, (uint16_t)length);
    }
}

/**
 * @brief 生成带序号的 ACK/NACK 文本，并复用行发送函数追加 CRLF。
 * name 为应答名称，sequence 为要报告的 16 位序号；输出格式为 名称,十进制序号。
 * 无返回值；name 为空或格式化失败、截断时不发送应答。
 */
static void OtaUart_SendSequenceResponse(const char *name, uint16_t sequence)
{
    char response[32];
    int length;

    if (name == NULL)
    {
        return;
    }
    length = snprintf(response, sizeof(response), "%s,%u",
                      name, (unsigned int)sequence);
    if ((length > 0) && ((uint32_t)length < sizeof(response)))
    {
        OtaUart_SendLine(response);
    }
}

/**
 * @brief 更新供界面和调试读取的 OTA 状态消息。
 * text 必须为有效的结束符结尾字符串；调用点均传入固定状态文本。
 * 无返回值；按内部缓冲容量截断并保证结束符，覆盖上一次消息。
 */
static void OtaUart_SetMessage(const char *text)
{
    (void)snprintf(uart.status_message, sizeof(uart.status_message), "%s", text);
}

/**
 * @brief 检查版本文本是否仅由协议允许的字符组成。
 * text 为版本字符串；只接受非空的字母、数字、点、下划线和连字符。
 * 合法返回 true，空指针、空字符串或非法字符返回 false；不修改输入。
 */
static bool OtaUart_VersionIsSafe(const char *text)
{
    uint32_t index;

    if ((text == NULL) || (text[0] == '\0'))
    {
        return false;
    }
    for (index = 0U; text[index] != '\0'; index++)
    {
        if ((!isalnum((unsigned char)text[index])) && (text[index] != '.') &&
            (text[index] != '_') && (text[index] != '-'))
        {
            return false;
        }
    }
    return true;
}

/**
 * @brief 校验摘要文本，并将其中的十六进制大写字母原地转为小写。
 * text 必须指向可写的 65 字节摘要缓冲区，不接受空指针。
 * 恰好 64 个十六进制字符且末尾为结束符返回 true，否则返回 false；失败前可能已转换前缀。
 */
static bool OtaUart_NormalizeSha256(char text[65])
{
    uint32_t index;

    for (index = 0U; index < 64U; index++)
    {
        if (!isxdigit((unsigned char)text[index]))
        {
            return false;
        }
        text[index] = (char)tolower((unsigned char)text[index]);
    }
    return text[64] == '\0';
}

/**
 * @brief 把一个 16 位无符号整数编码为两个小端字节。
 * output 必须指向至少 2 字节的可写缓冲区；value 为待编码数值。
 * 无返回值；低 8 位写入 output[0]，高 8 位写入 output[1]，用于构造 CRC 覆盖头部。
 */
static void OtaUart_WriteLe16(uint8_t output[2], uint16_t value)
{
    output[0] = (uint8_t)(value & 0xFFU);
    output[1] = (uint8_t)(value >> 8U);
}

#endif /* OTA_UART_HOST_TEST */

/**
 * @brief 计算串口帧使用的 CRC16，初值 0xFFFF，反射多项式 0xA001，无末尾异或。
 * data 指向 length 字节；空指针返回 0，非空指针配零长度返回初值。
 * 返回 16 位校验值；调用方提供序号、长度和负载的连续字节，不包含同步头及帧尾 CRC。
 */
uint16_t OtaUart_Crc16(const uint8_t *data, uint32_t length)
{
    uint16_t crc = 0xFFFFU;
    uint8_t bit;

    if (data == NULL)
    {
        return 0U;
    }

    while (length-- != 0U)
    {
        crc ^= *data++;
        for (bit = 0U; bit < 8U; bit++)
        {
            crc = ((crc & 1U) != 0U) ?
                  (uint16_t)((crc >> 1U) ^ 0xA001U) : (uint16_t)(crc >> 1U);
        }
    }
    return crc;
}

