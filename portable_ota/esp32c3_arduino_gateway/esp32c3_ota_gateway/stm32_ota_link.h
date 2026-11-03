/*
 * ESP32-C3 到 STM32 的可靠 OTA 串口链路。
 * beginTransfer/transfer/endTransfer 在 Arduino 主循环上下文执行并可能等待应答；
 * 二进制帧按序号重试，包内 String 数据在传输调用返回前必须保持有效。
 */
#pragma once

#include <Arduino.h>

struct Stm32OtaPackage
{
  String version;          /* 云端包版本，也是 STM32 确认启动后回报的版本。 */
  char targetSlot;         /* 只能是 'A' 或 'B'。 */
  uint32_t fileSize;       /* HTTP 数据总长度，必须为偶数且不超过槽位。 */
  String sha256;           /* 64 位十六进制摘要，由 STM32 对整包复核。 */
  String url;              /* IoTDA 平台地址或 OBS 预签名地址。 */
  String accessToken;      /* 平台存储包下载使用的临时 Bearer 令牌。 */
  bool needsAuthorization; /* true 时 HTTP 请求必须附带上述令牌。 */
};

typedef void (*Stm32OtaProgressCallback)(uint8_t progress);

class Stm32OtaLink
{
public:
  explicit Stm32OtaLink(HardwareSerial &serial);

  /* 初始化连接 STM32 的硬件串口；应在 setup() 中调用一次。 */
  void begin(int rxPin, int txPin, uint32_t baudRate);
  /* 非阻塞收集文本行；没有完整换行消息时返回 false。 */
  bool readLine(String &line);
  /* 发送 OTA_BEGIN，并等待 STM32 完成参数检查和目标槽擦除。 */
  bool beginTransfer(const Stm32OtaPackage &package);
  /* 从 HTTP 流读取 fileSize 字节，分帧发送并等待每帧 ACK。 */
  bool transfer(Stream &stream, uint32_t fileSize,
                Stm32OtaProgressCallback progressCallback);
  /* 发送 OTA_END；只有 STM32 完成整包校验并建立 pending 才返回 true。 */
  bool endTransfer();
  /* 通知 STM32 丢弃当前下载会话；不会修改已经确认的 active 槽。 */
  void abort();
  /* 失败时返回简短原因，供 LCD、COM5 和华为云状态页显示。 */
  const char *lastErrorText() const;

private:
  bool sendFrame(uint16_t sequence, const uint8_t *payload, uint16_t length);
  /* Wi-Fi 有时一次只给一部分字节；此函数会凑满一帧再返回。 */
  bool readExactly(Stream &stream, uint8_t *buffer, size_t length);
  bool waitForLine(const String &expected, uint32_t timeoutMs);
  uint16_t crc16(const uint8_t *data, size_t length) const;

  HardwareSerial &serial_; /* 引用 setup 中创建的 UART，不拥有其生命周期。 */
  String receivedLine_;    /* 跨多次 loop 保存尚未收到换行符的半行。 */
  const char *lastError_;  /* 指向静态错误短语，不需要释放。 */
};
