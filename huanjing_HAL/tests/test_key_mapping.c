/* 编译期测试：确认项目中定义的 KEY1 引脚与原理图一致。 */
#include "app_key.h"

/* 若没有定义 KEY1 引脚宏，编译必须失败，防止再次写死错误引脚。 */
#ifndef APP_KEY1_PIN
#error "APP_KEY1_PIN must be defined in app_key.h"
#endif

/* 原理图第 8 页：KEY1 连接到 PF8。 */
#if APP_KEY1_PIN != GPIO_PIN_8
#error "KEY1 must use PF8 according to the board schematic"
#endif

int main(void)
{
    return 0;
}
