#include "modbus_slave.h"
#include "bsp_usart.h"
#include "bsp_timer.h"
#include "modbus_frame.h"

/*
 * Modbus RTU 从机 —— 硬件侧。
 *
 * 这里的职责只剩两件：
 *   1. 从串口收字节，按 3.5 字符空闲断帧
 *   2. 把解析出的请求交给 modbus_frame 层处理，并把响应发回串口
 *
 * 协议逻辑全部在 modbus_frame.c（纯逻辑，PC 端可测）。
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

/* 处理一帧请求（frame 层已解析好，这里只做"业务规则 + 硬件收发"） */
static void HandleRequest(const uint8_t *frame, uint16_t len)
{
	MbParseResult parsed;
	uint8_t  resp[MODBUS_FRAME_MAX];
	uint16_t resp_len;
	uint16_t i;

	if (!ParseRequest(frame, len, MODBUS_SLAVE_ADDR, &parsed))
	{
		return;   /* CRC 错 / 地址不符 / 长度错：静默丢弃 */
	}

	switch (parsed.type)
	{
		case MB_FRAME_READ:
			/* 数量校验：1 ~ 125 */
			if (parsed.qty == 0 || parsed.qty > MODBUS_MAX_READ_QTY)
			{
				resp_len = BuildException(resp, parsed.slave_addr,
				                          MODBUS_FC_READ_HOLDING, MODBUS_EXC_ILLEGAL_VALUE);
				BSP_USART_SendBuf(resp, resp_len);
				return;
			}

			/* 地址校验：起始 + 数量不能越界 */
			if ((uint32_t)parsed.start + parsed.qty > MODBUS_REG_COUNT)
			{
				resp_len = BuildException(resp, parsed.slave_addr,
				                          MODBUS_FC_READ_HOLDING, MODBUS_EXC_ILLEGAL_ADDRESS);
				BSP_USART_SendBuf(resp, resp_len);
				return;
			}

			/* 正常读：填数据后加 CRC */
			resp_len = BuildReadHeader(resp, parsed.slave_addr, (uint8_t)parsed.qty);
			for (i = 0; i < parsed.qty; i++)
			{
				resp[resp_len + i * 2]     = (uint8_t)(s_regs[parsed.start + i] >> 8);   /* 大端 */
				resp[resp_len + i * 2 + 1] = (uint8_t)(s_regs[parsed.start + i] & 0xFF);
			}
			resp_len = AppendCrc(resp, resp_len + parsed.qty * 2);
			BSP_USART_SendBuf(resp, resp_len);
			break;

		case MB_FRAME_WRITE:
			/* 寄存器地址越界 -> 异常 0x02 */
			if (parsed.start >= MODBUS_REG_COUNT)
			{
				resp_len = BuildException(resp, parsed.slave_addr,
				                          MODBUS_FC_WRITE_SINGLE, MODBUS_EXC_ILLEGAL_ADDRESS);
				BSP_USART_SendBuf(resp, resp_len);
				return;
			}

			/* 写入 */
			s_regs[parsed.start] = parsed.value;

			/* 写成功回显整帧 */
			resp_len = BuildWriteResponse(resp, frame, len);
			BSP_USART_SendBuf(resp, resp_len);
			break;

		default:
			/* 未知功能码 -> 异常 0x01 */
			resp_len = BuildException(resp, parsed.slave_addr, parsed.func,
			                          MODBUS_EXC_ILLEGAL_FUNCTION);
			BSP_USART_SendBuf(resp, resp_len);
			break;
	}
}

void Modbus_Poll(void)
{
	uint8_t  ch;
	uint32_t now;
	uint32_t elapsed;

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

	/* 2. 空闲超时判断帧结束（MODBUS_T35_MS = 4ms，即 3.5 字符时间） */
	now = BSP_TIMER_GetMs();
	if (s_rx_len > 0)
	{
		elapsed = (uint32_t)(now - s_last_rx_ms);

		if (elapsed >= MODBUS_T35_MS)
		{
			HandleRequest(s_rx, s_rx_len);
			s_rx_len = 0;
		}
		/* 3. 兜底保护（BUG-001 修复）：异常字节流超时未处理，强制清空 */
		else if (elapsed >= MODBUS_RX_TIMEOUT_MS)
		{
			s_rx_len = 0;
		}
	}
}
