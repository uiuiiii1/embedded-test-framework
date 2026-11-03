#include "bsp_crc.h"

/*
 * 算法说明：
 *   初值 0xFFFF
 *   对每个字节：CRC ^= 字节，然后右移 8 次，
 *               每次若最低位为 1 则右移后异或 0xA001
 *
 * 标准测试向量：
 *   {01 03 00 00 00 0A} -> 0xCDC5
 *   {00}                -> 0x40BF
 *   空                  -> 0xFFFF
 */
uint16_t crc16_modbus(const uint8_t *data, uint16_t len)
{
	uint16_t crc = 0xFFFF;
	uint16_t i;
	uint8_t  bit;

	for (i = 0; i < len; i++)
	{
		crc ^= (uint16_t)data[i];

		for (bit = 0; bit < 8; bit++)
		{
			if (crc & 0x0001)
			{
				crc = (crc >> 1) ^ 0xA001;
			}
			else
			{
				crc = crc >> 1;
			}
		}
	}

	return crc;
}
