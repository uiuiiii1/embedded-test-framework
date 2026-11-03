#ifndef __BSP_USART_H
#define __BSP_USART_H

#include "stm32f10x.h"

/* 接收环形缓冲区大小 */
#define USART_RX_BUF_SIZE   64

void BSP_USART_Init(uint32_t baudrate);
void BSP_USART_SendByte(uint8_t byte);
void BSP_USART_SendBuf(const uint8_t *buf, uint16_t len);

/* 查询接收：有数据返回 1 并通过 *byte 输出，无数据返回 0 */
uint8_t BSP_USART_ReadByte(uint8_t *byte);

/* 缓冲区里当前可读的字节数 */
uint16_t BSP_USART_Available(void);

#endif
