#ifndef __BSP_CRC_H
#define __BSP_CRC_H

#include "stm32f10x.h"

/*
 * Modbus RTU 标准 CRC16
 * 多项式 0xA001（标准多项式 0x8005 的位反转形式），初值 0xFFFF
 * 返回 16 位整数，追加到帧尾时低字节在前
 */
uint16_t crc16_modbus(const uint8_t *data, uint16_t len);

#endif
