#ifndef __BSP_TIMER_H
#define __BSP_TIMER_H

#include "stm32f10x.h"

/*
 * TIM2 提供 1ms 递增计数，用于 Modbus 的 3.5 字符空闲断帧判断
 * 1ms 中断频率：72MHz / 7200 / 10 = 1000Hz
 */
void     BSP_TIMER_Init(void);
uint32_t BSP_TIMER_GetMs(void);

#endif
