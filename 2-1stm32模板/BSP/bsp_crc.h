#ifndef __BSP_CRC_H
#define __BSP_CRC_H

#include "stm32f10x.h"
#include "modbus_crc.h"

/*
 * 固件侧统一从这里引用 CRC16。
 *
 * 实际算法在 modbus_crc.c 里（纯 C、不依赖硬件头文件），
 * 好处是 PC 端单元测试和 STM32 固件编译的是同一份代码，
 * gcov 统计出来的行覆盖率真实对应固件里跑的代码。
 *
 * 函数声明见 modbus_crc.h：
 *     uint16_t crc16_modbus(const uint8_t *data, uint16_t len);
 */

#endif
