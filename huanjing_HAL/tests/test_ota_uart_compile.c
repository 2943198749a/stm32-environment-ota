#include <assert.h>
#include <stdint.h>
#include "ota_uart.h"
#include <string.h>

/* 帧构造和验证仅供此测试使用，不再放进正式固件。 */
/* 按低字节在前写入 16 位数，和 ESP32 发送格式保持一致。 */
static void OtaUart_WriteLe16(uint8_t output[2], uint16_t value)
{
    output[0] = (uint8_t)(value & 0xFFU);
    output[1] = (uint8_t)(value >> 8U);
}

/* 从低字节在前的两个字节读出 16 位数。 */
static uint16_t OtaUart_ReadLe16(const uint8_t input[2])
{
    return (uint16_t)input[0] | ((uint16_t)input[1] << 8U);
}

static uint32_t OtaUart_BuildFrameForTest(uint16_t sequence, const uint8_t *payload,
                                   uint16_t length, uint8_t *frame,
                                   uint32_t frame_size)
{
    uint16_t crc;
    uint32_t total_size = OTA_UART_FRAME_OVERHEAD + length;

    if ((payload == NULL) || (frame == NULL) || (length == 0U) ||
        (length > OTA_UART_MAX_PAYLOAD) || (frame_size < total_size))
    {
        return 0U;
    }

    frame[0] = OTA_UART_SYNC_1;
    frame[1] = OTA_UART_SYNC_2;
    OtaUart_WriteLe16(&frame[2], sequence);
    OtaUart_WriteLe16(&frame[4], length);
    memcpy(&frame[6], payload, length);
    /* 直接对连续的“序号+长度+数据”算一次，避免 CRC 分段规则差异。 */
    crc = OtaUart_Crc16(&frame[2], (uint32_t)length + 4U);
    OtaUart_WriteLe16(&frame[6U + length], crc);
    return total_size;
}

static bool OtaUart_FrameIsValidForTest(const uint8_t *frame, uint32_t frame_size)
{
    uint16_t length;
    uint16_t expected_crc;
    uint16_t actual_crc;

    if ((frame == NULL) || (frame_size < OTA_UART_FRAME_OVERHEAD) ||
        (frame[0] != OTA_UART_SYNC_1) || (frame[1] != OTA_UART_SYNC_2))
    {
        return false;
    }

    length = OtaUart_ReadLe16(&frame[4]);
    if ((length == 0U) || (length > OTA_UART_MAX_PAYLOAD) ||
        (frame_size != (uint32_t)length + OTA_UART_FRAME_OVERHEAD))
    {
        return false;
    }

    expected_crc = OtaUart_ReadLe16(&frame[6U + length]);
    actual_crc = OtaUart_Crc16(&frame[2], (uint32_t)length + 4U);
    return expected_crc == actual_crc;
}

int main(void)
{
    static const uint8_t modbus_vector[] = "123456789";
    uint8_t payload[4] = {0x01U, 0x02U, 0x03U, 0x04U};
    uint8_t frame[OTA_UART_FRAME_OVERHEAD + sizeof(payload)];
    uint32_t size;

    assert(OtaUart_Crc16(modbus_vector, 9U) == 0x4B37U);
    size = OtaUart_BuildFrameForTest(7U, payload, sizeof(payload),
                                     frame, sizeof(frame));
    assert(size == sizeof(frame));
    assert(OtaUart_FrameIsValidForTest(frame, size));
    frame[6] ^= 0x01U;
    assert(!OtaUart_FrameIsValidForTest(frame, size));
    return 0;
}
