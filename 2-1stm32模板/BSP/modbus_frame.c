#include "modbus_frame.h"
#include "modbus_crc.h"

/*
 * Modbus RTU 帧解析 —— 纯逻辑实现。
 *
 * 这个文件不 include stm32 头文件，只依赖 modbus_crc.h（纯算法）。
 * PC 端单元测试用 gcc 直接编译它，无需 Keil。
 */

/* 3.5 字符空闲对应的功能码长度表：0x03/0x06 请求固定 8 字节 */
#define MB_REQ_LEN_FIXED   8

/* 异常响应帧长 */
#define MB_EXC_FRAME_LEN   5


uint8_t ParseRequest(const uint8_t *frame, uint16_t len,
                     uint8_t my_addr, MbParseResult *out)
{
	uint16_t crc_calc;
	uint16_t crc_recv;

	if (frame == 0 || out == 0)
	{
		return 0;
	}

	/* 最短合法帧：地址(1) + 功能码(1) + CRC(2) = 4 字节 */
	if (len < 4)
	{
		return 0;
	}

	/* 1. CRC 校验（帧尾 2 字节，低字节在前） */
	crc_calc = crc16_modbus(frame, len - 2);
	crc_recv = (uint16_t)frame[len - 1] << 8 | frame[len - 2];
	if (crc_calc != crc_recv)
	{
		return 0;
	}

	out->slave_addr = frame[0];
	out->func       = frame[1];

	/* 2. 地址必须是自己 */
	if (out->slave_addr != my_addr)
	{
		return 0;
	}

	/* 3. 按功能码解析 */
	switch (out->func)
	{
		case MB_FRAME_READ:
			/* 请求必须 8 字节：[addr][03][startHi][startLo][qtyHi][qtyLo][crcLo][crcHi] */
			if (len != MB_REQ_LEN_FIXED)
			{
				return 0;
			}
			out->type  = MB_FRAME_READ;
			out->start = (uint16_t)frame[2] << 8 | frame[3];
			out->qty   = (uint16_t)frame[4] << 8 | frame[5];
			return 1;

		case MB_FRAME_WRITE:
			/* 请求必须 8 字节：[addr][06][regHi][regLo][valHi][valLo][crcLo][crcHi] */
			if (len != MB_REQ_LEN_FIXED)
			{
				return 0;
			}
			out->type  = MB_FRAME_WRITE;
			out->start = (uint16_t)frame[2] << 8 | frame[3];
			out->value = (uint16_t)frame[4] << 8 | frame[5];
			return 1;

		default:
			/* 未知功能码：能认出帧头，但类型标记为 UNKNOWN，交给上层回异常 */
			out->type = MB_FRAME_UNKNOWN;
			return 1;
	}
}


uint16_t AppendCrc(uint8_t *frame, uint16_t len)
{
	uint16_t crc = crc16_modbus(frame, len);

	frame[len]     = (uint8_t)(crc & 0xFF);        /* 低字节在前 */
	frame[len + 1] = (uint8_t)(crc >> 8);

	return len + 2;
}


uint16_t BuildReadHeader(uint8_t *resp, uint8_t slave_addr, uint8_t qty)
{
	/* 响应头：[addr][03][byteCount]，byteCount = qty * 2 */
	resp[0] = slave_addr;
	resp[1] = MB_FRAME_READ;
	resp[2] = (uint8_t)(qty * 2);

	return 3;
}


uint16_t BuildWriteResponse(uint8_t *resp, const uint8_t *req, uint16_t len)
{
	uint16_t i;

	for (i = 0; i < len; i++)
	{
		resp[i] = req[i];
	}

	return len;
}


uint16_t BuildException(uint8_t *resp, uint8_t slave_addr, uint8_t func, uint8_t exc)
{
	resp[0] = slave_addr;
	resp[1] = (uint8_t)(func | 0x80);   /* 功能码最高位置 1 */
	resp[2] = exc;

	return AppendCrc(resp, 3);
}
