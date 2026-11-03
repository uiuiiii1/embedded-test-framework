#include "bsp_timer.h"

/* 上电以来的毫秒计数，中断里自增 */
static volatile uint32_t s_ms_tick = 0;

void BSP_TIMER_Init(void)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	NVIC_InitTypeDef        NVIC_InitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

	/* 72MHz / 7200 = 10kHz，再数 10 次 = 1kHz = 1ms */
	TIM_TimeBaseStructure.TIM_Prescaler         = 7200 - 1;
	TIM_TimeBaseStructure.TIM_Period            = 10 - 1;
	TIM_TimeBaseStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

	TIM_ClearFlag(TIM2, TIM_FLAG_Update);
	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

	NVIC_InitStructure.NVIC_IRQChannel                   = TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	TIM_Cmd(TIM2, ENABLE);

	s_ms_tick = 0;
}

uint32_t BSP_TIMER_GetMs(void)
{
	return s_ms_tick;
}

void TIM2_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
	{
		s_ms_tick++;
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
	}
}
