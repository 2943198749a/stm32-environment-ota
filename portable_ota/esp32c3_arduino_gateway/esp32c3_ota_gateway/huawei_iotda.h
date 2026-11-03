/*
 * 华为云 IoTDA 会话封装。
 * update() 维护 Wi-Fi/MQTT 并处理下行事件；解析出的待升级包由
 * takePendingPackage() 按值交给调用者。类不会把设备密钥或访问令牌写入日志。
 */
#pragma once

#include <PubSubClient.h>
#include <WiFi.h>
#include "stm32_ota_link.h"

class HuaweiIotda
{
public:
  /* 构造时只初始化本地状态，不会立即连接网络。 */
  HuaweiIotda();

  /* 配置 Wi-Fi/MQTT 客户端；在 Arduino setup() 中调用一次。 */
  void begin();
  /* 非阻塞维护连接和 MQTT 心跳；必须从 loop() 高频调用。 */
  void update();
  /* 把 STM32 的 ENV 文本解析成华为云属性 JSON 并发布。 */
  bool publishEnvironment(const String &envLine);
  /* 按值取走一笔待处理 OTA；成功后内部 pending 标志自动清除。 */
  bool takePendingPackage(Stm32OtaPackage &package);
  /* description 会显示为华为云 OTA 任务的设备侧失败原因。 */
  void reportProgress(const String &version, uint8_t progress, int resultCode,
                      const String &description = "");
  void reportCurrentVersion();
  void setCurrentVersion(const String &version);
  bool wifiConnected();
  bool mqttConnected();
  bool lastPublishOk();

private:
  static void mqttCallback(char *topic, uint8_t *payload, unsigned int length);
  void handleMqttMessage(const uint8_t *payload, unsigned int length);
  void ensureWifi();
  void ensureMqtt();
  bool publishJson(const String &topic, const String &json);

  WiFiClient wifiClient_;            /* 提供 MQTT 所需的 TCP 连接。 */
  PubSubClient mqttClient_;          /* 在 TCP 上实现 MQTT 协议。 */
  Stm32OtaPackage pendingPackage_;   /* MQTT 回调解析出的下一笔升级任务。 */
  bool packagePending_;              /* 防止未取走任务被当成空包。 */
  bool lastPublishOk_;               /* 最近一次环境属性发布结果。 */
  bool wifiConnecting_;              /* 避免连接尚未结束时重复 WiFi.begin。 */
  String currentVersion_;            /* STM32 当前已确认版本。 */
  uint32_t lastWifiAttemptMs_;       /* Wi-Fi 重试节流时间戳。 */
  uint32_t lastMqttAttemptMs_;       /* MQTT 重试节流时间戳。 */
  static HuaweiIotda *instance_;     /* 把 C 风格 MQTT 回调转发给当前对象。 */
};
