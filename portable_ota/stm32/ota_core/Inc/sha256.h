/*
 * 可增量输入的 SHA-256，用于校验华为云 OTA 包。
 * 生命周期固定为 Init -> 一次或多次 Update -> Final；同一个上下文不能由
 * 多个任务并发修改。二进制摘要为 32 字节，对应文本为 64 个十六进制字符。
 */
#ifndef __SHA256_H
#define __SHA256_H

#include <stdbool.h>
#include <stdint.h>

/* SHA-256 的运行状态，可连续输入多个固件数据块。 */
typedef struct
{
    uint32_t state[8];       /* 八个 32 位链式状态，每处理一块都会更新。 */
    uint64_t total_length;   /* 从 Init 起累计输入的总字节数。 */
    uint8_t buffer[64];      /* 尚未凑满一个 512-bit 分组的尾部数据。 */
    uint32_t buffer_length;  /* buffer 中当前有效字节数，范围 0~63。 */
} Sha256Context_t;

/* 初始化 context 的全部状态；context 不能为 NULL。 */
void Sha256_Init(Sha256Context_t *context);
/* 追加 length 字节；context/data 在 length 非零时都必须有效，函数不保存 data。 */
void Sha256_Update(Sha256Context_t *context, const uint8_t *data, uint32_t length);
/* 完成填充和计算，将 32 字节摘要写入 digest；调用后应重新 Init 才能复用。 */
void Sha256_Final(Sha256Context_t *context, uint8_t digest[32]);
/* 比较摘要与 64 位十六进制文本；参数无效或字符不合法时返回 false。 */
bool Sha256_HexEquals(const uint8_t digest[32], const char expected_hex[65]);

#endif
