#include "bsp_usart.h"

/*
 * 串口1 初始化
 * 引脚：PA9  = USART1_TX（复用推挽输出）
 *       PA10 = USART1_RX（上拉输入）
 * 格式：8 数据位，无校验，1 停止位
 * 中断：只开启接收中断 RXNE
 */

/* 接收环形缓冲区 */
static uint8_t  s_rx_buf[USART_RX_BUF_SIZE];
static volatile uint16_t s_rx_head = 0;   /* 中断写入位置 */
static volatile uint16_t s_rx_tail = 0;   /* 主循环读取位置 */

void BSP_USART_Init(uint32_t baudrate)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef  NVIC_InitStructure;

	/* 1. 打开 GPIOA 和 USART1 时钟 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

	/* 2. PA9 = TX */
	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* 3. PA10 = RX */
	GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* 4. 串口参数 */
	USART_InitStructure.USART_BaudRate            = baudrate;
	USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits            = USART_StopBits_1;
	USART_InitStructure.USART_Parity              = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART1, &USART_InitStructure);

	/* 5. 使能接收中断 */
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

	NVIC_InitStructure.NVIC_IRQChannel                   = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	/* 6. 使能串口 */
	USART_Cmd(USART1, ENABLE);

	s_rx_head = 0;
	s_rx_tail = 0;
}

void BSP_USART_SendByte(uint8_t byte)
{
	/* 等待发送数据寄存器空 */
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
	USART_SendData(USART1, byte);
}

void BSP_USART_SendBuf(const uint8_t *buf, uint16_t len)
{
	uint16_t i;
	for (i = 0; i < len; i++)
	{
		BSP_USART_SendByte(buf[i]);
	}
	/* 等待最后一个字节真正发完（TC 标志） */
	while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
}

uint8_t BSP_USART_ReadByte(uint8_t *byte)
{
	if (s_rx_tail == s_rx_head)
	{
		return 0;   /* 缓冲区空 */
	}
	*byte = s_rx_buf[s_rx_tail];
	s_rx_tail = (s_rx_tail + 1) % USART_RX_BUF_SIZE;
	return 1;
}

uint16_t BSP_USART_Available(void)
{
	return (uint16_t)((s_rx_head + USART_RX_BUF_SIZE - s_rx_tail) % USART_RX_BUF_SIZE);
}

/* 串口1 中断服务函数：只负责把接收到的字节塞进环形缓冲区 */
void USART1_IRQHandler(void)
{
	uint8_t data;
	uint16_t next;

	if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
	{
		data = (uint8_t)USART_ReceiveData(USART1);

		next = (s_rx_head + 1) % USART_RX_BUF_SIZE;
		if (next != s_rx_tail)          /* 缓冲区没满才存，满了直接丢弃 */
		{
			s_rx_buf[s_rx_head] = data;
			s_rx_head = next;
		}

		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
	}
}
