/* Bootloader 对外只暴露“选择槽位”和“跳转槽位”两个动作。 */
#ifndef PORTABLE_BOOTLOADER_H
#define PORTABLE_BOOTLOADER_H
#include "ota_layout.h"
OtaSlot_t Bootloader_SelectSlot(void);
void Bootloader_JumpToSlot(OtaSlot_t slot);
#endif
