#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "bsp_usart.h"
#include "bsp_crc.h"
#include "bsp_timer.h"
#include "modbus_slave.h"

/*
 * 第 3 步：Modbus RTU 从机 —— 实现 0x03 读保持寄存器
 *
 * 参数：9600 8N1，从机地址 1
 * 寄存器：100 个，初值 regs[i] = i
 *
 * 测试帧（带 CRC，可直接复制到串口助手 HEX 发送）：
 *   读寄存器 0 开始的 4 个：
 *     01 03 00 00 00 04 44 09
 *   期望响应（寄存器 0,1,2,3 的值分别是 0,1,2,3）：
 *     01 03 08 00 00 00 01 00 02 00 03 49 D6
 *
 *   读寄存器 0 开始的 10 个：
 *     01 03 00 00 00 0A C5 CD
 */
int main(void)
{
	/* 原来的点灯配置保留 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	GPIO_InitTypeDef GPIO_InitStructre;
	GPIO_InitStructre.GPIO_Mode  = GPIO_Mode_Out_PP;
	GPIO_InitStructre.GPIO_Pin   = GPIO_Pin_13;
	GPIO_InitStructre.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructre);
	GPIO_SetBits(GPIOC, GPIO_Pin_13);

	/* 1毫秒时基 */
	BSP_TIMER_Init();

	/* 串口1，9600 8N1 */
	BSP_USART_Init(9600);

	/* 从机初始化 */
	Modbus_Init();

	while (1)
	{
		Modbus_Poll();
	}
}
