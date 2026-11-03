#include "modbus_slave.h"
#include "bsp_usart.h"
#include "bsp_crc.h"
#include "bsp_timer.h"

/*
 * Modbus RTU 从机（第 3 步：只实现 0x03 读保持寄存器）
 *
 * 断帧策略：空闲超时
 *   收到第一个字节后开始计时，每收到一个字节就把"最后收字节时刻"刷新；
 *   连续 MODBUS_T35_MS(4ms) 没有新字节，判定一帧结束，开始解析。
 *   这是 Modbus RTU 标准规定的做法。
 */

/* 保持寄存器表 */
static uint16_t s_regs[MODBUS_REG_COUNT];

/* 接收缓冲 */
static uint8_t  s_rx[MODBUS_FRAME_MAX];
static uint16_t s_rx_len = 0;
static uint32_t s_last_rx_ms = 0;

void Modbus_Init(void)
{
	uint16_t i;

	/* 寄存器初值：regs[i] = i，便于肉眼识别 */
	for (i = 0; i < MODBUS_REG_COUNT; i++)
	{
		s_regs[i] = i;
	}

	s_rx_len = 0;
	s_last_rx_ms = 0;
}

uint16_t Modbus_GetReg(uint16_t index)
{
	if (index >= MODBUS_REG_COUNT)
	{
		return 0;
	}
	return s_regs[index];
}

/* 发送异常响应：[addr][func|0x80][excCode][crcLo][crcHi] */
static void SendException(uint8_t addr, uint8_t func, uint8_t exc)
{
	uint8_t  frame[5];
	uint16_t crc;

	frame[0] = addr;
	frame[1] = func | 0x80;
	frame[2] = exc;

	crc = crc16_modbus(frame, 3);
	frame[3] = crc & 0xFF;
	frame[4] = (crc >> 8) & 0xFF;

	BSP_USART_SendBuf(frame, 5);
}

/* 处理 0x03 读保持寄存器 */
static void HandleReadHolding(const uint8_t *req, uint16_t len)
{
	uint8_t  addr     = req[0];
	uint16_t start    = ((uint16_t)req[2] << 8) | req[3];
	uint16_t qty      = ((uint16_t)req[4] << 8) | req[5];
	uint8_t  resp[MODBUS_FRAME_MAX];
	uint16_t crc;
	uint16_t i;

	/* 请求长度必须是 8 字节 */
	if (len != 8)
	{
		return;
	}

	/* 数量校验：1 ~ 125 */
	if (qty == 0 || qty > MODBUS_MAX_READ_QTY)
	{
		SendException(addr, MODBUS_FC_READ_HOLDING, MODBUS_EXC_ILLEGAL_VALUE);
		return;
	}

	/* 地址校验：起始 + 数量不能越界 */
	if ((uint32_t)start + qty > MODBUS_REG_COUNT)
	{
		SendException(addr, MODBUS_FC_READ_HOLDING, MODBUS_EXC_ILLEGAL_ADDRESS);
		return;
	}

	/* 组响应：[addr][03][byteCount][data...][crc] */
	resp[0] = addr;
	resp[1] = MODBUS_FC_READ_HOLDING;
	resp[2] = (uint8_t)(qty * 2);

	for (i = 0; i < qty; i++)
	{
		resp[3 + i * 2]     = (s_regs[start + i] >> 8) & 0xFF;   /* 大端：高字节在前 */
		resp[3 + i * 2 + 1] = s_regs[start + i] & 0xFF;
	}

	crc = crc16_modbus(resp, 3 + qty * 2);
	resp[3 + qty * 2]     = crc & 0xFF;
	resp[3 + qty * 2 + 1] = (crc >> 8) & 0xFF;

	BSP_USART_SendBuf(resp, 3 + qty * 2 + 2);
}

/* 处理 0x06 写单个寄存器 */
static void HandleWriteSingle(const uint8_t *req, uint16_t len)
{
	uint8_t  addr  = req[0];
	uint16_t reg   = ((uint16_t)req[2] << 8) | req[3];
	uint16_t value = ((uint16_t)req[4] << 8) | req[5];

	/* 请求长度必须是 8 字节 */
	if (len != 8)
	{
		return;
	}

	/* 寄存器地址越界 -> 异常 0x02 */
	if (reg >= MODBUS_REG_COUNT)
	{
		SendException(addr, MODBUS_FC_WRITE_SINGLE, MODBUS_EXC_ILLEGAL_ADDRESS);
		return;
	}

	/* 写入 */
	s_regs[reg] = value;

	/* Modbus 规定写成功要原样回显整个请求帧 */
	BSP_USART_SendBuf(req, len);
}

/* 解析并处理一帧完整请求 */
static void ProcessFrame(const uint8_t *frame, uint16_t len)
{
	uint16_t crc_calc;
	uint16_t crc_recv;
	uint8_t  addr;
	uint8_t  func;

	/* 最短帧：地址 + 功能码 + CRC = 4 字节 */
	if (len < 4)
	{
		return;
	}

	/* CRC 校验：不通过直接丢弃，不应答 */
	crc_calc = crc16_modbus(frame, len - 2);
	crc_recv = ((uint16_t)frame[len - 1] << 8) | frame[len - 2];   /* 低字节在前 */
	if (crc_calc != crc_recv)
	{
		return;
	}

	addr = frame[0];
	func = frame[1];

	/* 地址不是自己（广播地址 0 暂不支持），不应答 */
	if (addr != MODBUS_SLAVE_ADDR)
	{
		return;
	}

	switch (func)
	{
		case MODBUS_FC_READ_HOLDING:
			HandleReadHolding(frame, len);
			break;

		case MODBUS_FC_WRITE_SINGLE:
			HandleWriteSingle(frame, len);
			break;

		default:
			SendException(addr, func, MODBUS_EXC_ILLEGAL_FUNCTION);
			break;
	}
}

void Modbus_Poll(void)
{
	uint8_t  ch;

	/* 1. 把串口缓冲里所有字节搬到自己的接收缓冲 */
	while (BSP_USART_ReadByte(&ch))
	{
		if (s_rx_len < MODBUS_FRAME_MAX)
		{
			s_rx[s_rx_len++] = ch;
		}
		else
		{
			/* 超长，丢弃整帧重新开始 */
			s_rx_len = 0;
		}
		s_last_rx_ms = BSP_TIMER_GetMs();
	}

	/* 2. 空闲超时判断帧结束 */
	if (s_rx_len > 0)
	{
		if ((BSP_TIMER_GetMs() - s_last_rx_ms) >= MODBUS_T35_MS)
		{
			ProcessFrame(s_rx, s_rx_len);
			s_rx_len = 0;
		}
	}
}
