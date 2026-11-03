#ifndef __MODBUS_CRC_H
#define __MODBUS_CRC_H

#include <stdint.h>

/*
 * Modbus RTU CRC16 —— 纯算法实现，不依赖任何硬件头文件。
 *
 * 这么做的目的：
 *   1. STM32 固件和 PC 端单元测试可以编译同一份代码
 *   2. gcov 能在 PC 上统计它的行覆盖率
 *
 * 多项式 0xA001（标准多项式 0x8005 的位反转形式），初值 0xFFFF。
 * 返回值追加到帧尾时低字节在前。
 */
uint16_t crc16_modbus(const uint8_t *data, uint16_t len);

#endif
