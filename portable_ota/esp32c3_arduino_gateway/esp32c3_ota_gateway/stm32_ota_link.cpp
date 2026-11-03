/*
 * STM32 OTA 帧发送实现：每帧都等待 ACK，失败最多重发三次。
 *
 * 一次传输分为 beginTransfer、transfer、endTransfer 三阶段。数据阶段把 HTTP
 * 流拆成最多 512 字节的小帧，每帧带连续序号和 CRC16；只有收到对应 ACK 才
 * 前进到下一帧。这样丢帧、短读或 STM32 Flash 写入失败都不会静默跳过数据。
 */
#include "stm32_ota_link.h"

static const uint8_t OTA_SYNC_1 = 0xA5U;
static const uint8_t OTA_SYNC_2 = 0x5AU;
static const uint16_t OTA_FRAME_MAX_PAYLOAD = 512U;

Stm32OtaLink::Stm32OtaLink(HardwareSerial &serial) : serial_(serial),
  lastError_("none")
{
}

void Stm32OtaLink::begin(int rxPin, int txPin, uint32_t baudRate)
{
  serial_.begin(baudRate, SERIAL_8N1, rxPin, txPin);
}

bool Stm32OtaLink::readLine(String &line)
{
  /* 使用成员缓冲区跨 loop 调用保留半行；遇到 \n 才交付一条完整消息。 */
  while (serial_.available() > 0)
  {
    char value = static_cast<char>(serial_.read());
    if (value == '\n')
    {
      line = receivedLine_;
      receivedLine_ = "";
      line.trim();
      return line.length() > 0;
    }
    if (value != '\r')
    {
      if (receivedLine_.length() < 160U)
      {
        receivedLine_ += value;
      }
      else
      {
        receivedLine_ = "";
      }
    }
  }
  return false;
}

uint16_t Stm32OtaLink::crc16(const uint8_t *data, size_t length) const
{
  /* CRC16/MODBUS：初值 0xFFFF，多项式反射形式 0xA001。 */
  uint16_t crc = 0xFFFFU;
  while (length-- > 0U)
  {
    crc ^= *data++;
    for (uint8_t bit = 0U; bit < 8U; bit++)
    {
      crc = (crc & 1U) ? static_cast<uint16_t>((crc >> 1U) ^ 0xA001U)
                        : static_cast<uint16_t>(crc >> 1U);
    }
  }
  return crc;
}

bool Stm32OtaLink::waitForLine(const String &expected, uint32_t timeoutMs)
{
  uint32_t start = millis();
  String line;
  while ((millis() - start) < timeoutMs)
  {
    if (readLine(line))
    {
      /* 只接受本次操作预期的精确应答；旧状态行不能冒充 ACK。 */
      if (line == expected)
      {
        return true;
      }
      if (line.startsWith("OTA_ERROR") || line.startsWith("OTA_NACK"))
      {
        return false;
      }
    }
    delay(1);
  }
  return false;
}

bool Stm32OtaLink::beginTransfer(const Stm32OtaPackage &package)
{
  String command = "OTA_BEGIN," + package.version + "," + package.targetSlot +
                   "," + String(package.fileSize) + "," + package.sha256 + "\r\n";
  serial_.print(command);
  if (!waitForLine("OTA_READY", 5000U))
  {
    lastError_ = "stm32_ready";
    return false;
  }
  lastError_ = "none";
  return true;
}

bool Stm32OtaLink::sendFrame(uint16_t sequence, const uint8_t *payload,
                             uint16_t length)
{
  uint8_t frame[OTA_FRAME_MAX_PAYLOAD + 8U];
  uint16_t crc;
  String expectedAck = "OTA_ACK," + String(sequence);

  if ((payload == nullptr) || (length == 0U) || (length > OTA_FRAME_MAX_PAYLOAD) ||
      ((length & 1U) != 0U))
  {
    lastError_ = "frame_data";
    return false;
  }

  /* 所有多字节字段按小端发送，与 STM32 OtaUart_ReadLe16 保持一致。 */
  frame[0] = OTA_SYNC_1;
  frame[1] = OTA_SYNC_2;
  frame[2] = static_cast<uint8_t>(sequence & 0xFFU);
  frame[3] = static_cast<uint8_t>(sequence >> 8U);
  frame[4] = static_cast<uint8_t>(length & 0xFFU);
  frame[5] = static_cast<uint8_t>(length >> 8U);
  memcpy(&frame[6], payload, length);
  crc = crc16(&frame[2], static_cast<size_t>(length) + 4U);
  frame[6U + length] = static_cast<uint8_t>(crc & 0xFFU);
  frame[7U + length] = static_cast<uint8_t>(crc >> 8U);

  serial_.write(frame, static_cast<size_t>(length) + 8U);
  /* STM32 需把一帧写入内部 Flash 后才回复，5 秒能覆盖偶发的 Flash 忙。 */
  if (!waitForLine(expectedAck, 5000U))
  {
    lastError_ = "frame_ack";
    return false;
  }
  return true;
}

bool Stm32OtaLink::readExactly(Stream &stream, uint8_t *buffer, size_t length)
{
  size_t received = 0U;
  uint32_t start = millis();

  /* readBytes() 允许短读，不能因第一次不足 512 字节就直接判下载失败。 */
  while (received < length)
  {
    size_t count = stream.readBytes(&buffer[received], length - received);
    received += count;
    if (received >= length)
    {
      return true;
    }
    if ((millis() - start) >= 15000U)
    {
      lastError_ = "http_read";
      return false;
    }
    delay(1U);
  }
  return true;
}

bool Stm32OtaLink::transfer(Stream &stream, uint32_t fileSize,
                            Stm32OtaProgressCallback progressCallback)
{
  uint8_t buffer[OTA_FRAME_MAX_PAYLOAD];
  uint32_t received = 0U;
  uint16_t sequence = 0U;

  while (received < fileSize)
  {
    uint16_t wanted = static_cast<uint16_t>(min<uint32_t>(OTA_FRAME_MAX_PAYLOAD,
                                                           fileSize - received));
    if (!readExactly(stream, buffer, wanted))
    {
      return false;
    }

    /* 同一 sequence 最多发送三次；STM32 只会写入期望序号，避免重复写。 */
    bool accepted = false;
    for (uint8_t attempt = 0U; attempt < 3U; attempt++)
    {
      if (sendFrame(sequence, buffer, wanted))
      {
        accepted = true;
        break;
      }
    }
    if (!accepted)
    {
      return false;
    }

    received += wanted;
    sequence++;
    if (progressCallback != nullptr)
    {
      progressCallback(static_cast<uint8_t>((received * 99UL) / fileSize));
    }
  }
  return true;
}

bool Stm32OtaLink::endTransfer()
{
  /* OTA_OK 表示 STM32 已完成整包 SHA-256/向量表检查并写入 pending。 */
  serial_.print("OTA_END\r\n");
  if (!waitForLine("OTA_OK", 8000U))
  {
    lastError_ = "stm32_verify";
    return false;
  }
  return true;
}

void Stm32OtaLink::abort()
{
  serial_.print("OTA_ABORT\r\n");
}

const char *Stm32OtaLink::lastErrorText() const
{
  return lastError_;
}
