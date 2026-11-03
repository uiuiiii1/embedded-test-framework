#ifndef __MODBUS_SLAVE_H
#define __MODBUS_SLAVE_H

#include "stm32f10x.h"

/* 从机地址 */
#define MODBUS_SLAVE_ADDR       0x01

/* 保持寄存器数量 */
#define MODBUS_REG_COUNT        100

/* 帧最大长度（Modbus RTU 标准上限 256 字节） */
#define MODBUS_FRAME_MAX        256

/*
 * 3.5 字符空闲间隔：
 * 9600 baud，每字符 11 bit（1起始+8数据+1校验+1停止）
 * 1 字符 = 11/9600 s ≈ 1.146ms
 * 3.5 字符 ≈ 4.0ms
 */
#define MODBUS_T35_MS           4

/* 功能码 */
#define MODBUS_FC_READ_HOLDING  0x03
#define MODBUS_FC_WRITE_SINGLE  0x06

/* 异常码 */
#define MODBUS_EXC_ILLEGAL_FUNCTION    0x01
#define MODBUS_EXC_ILLEGAL_ADDRESS     0x02
#define MODBUS_EXC_ILLEGAL_VALUE       0x03

/* 寄存器数量上限（0x03 一次最多读 125 个） */
#define MODBUS_MAX_READ_QTY     125

void     Modbus_Init(void);
void     Modbus_Poll(void);
uint16_t Modbus_GetReg(uint16_t index);

#endif

