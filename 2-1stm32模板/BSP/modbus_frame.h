#ifndef __MODBUS_FRAME_H
#define __MODBUS_FRAME_H

#include <stdint.h>

/*
 * Modbus RTU 帧解析 —— 纯逻辑层。
 *
 * 这个文件**不 include 任何 STM32 头文件**，
 * 只依赖 <stdint.h>，所以 PC 端和板子上都能编译。
 *
 * 目的：把"解析报文"和"操作硬件"分离。
 *   - 解析（本文件）：纯逻辑，PC 端用 Unity + gcov 测覆盖率
 *   - 操作（modbus_slave.c）：从串口搬字节、调本文件、把结果发出去
 */

/* 帧解析结果类型 */
typedef enum {
	MB_FRAME_READ   = 0x03,   /* 0x03 读保持寄存器 */
	MB_FRAME_WRITE  = 0x06,   /* 0x06 写单个寄存器 */
	MB_FRAME_UNKNOWN = 0xFF   /* 无法识别（长度错/CRC 错/地址不符/未知功能码） */
} MbFrameType;

/* 解析结果 */
typedef struct {
	MbFrameType type;
	uint8_t  slave_addr;   /* 帧里的从机地址 */
	uint8_t  func;         /* 功能码（原始） */
	uint16_t start;        /* 读：起始地址 / 写：寄存器地址 */
	uint16_t qty;          /* 读：数量 */
	uint16_t value;        /* 写：写入值 */
} MbParseResult;

/*
 * 解析一帧请求。
 *
 * @param frame  收到的完整字节（已按 3.5 字符间隔断帧）
 * @param len    frame 的字节数
 * @param my_addr 本机从机地址（不是自己就返回 UNKNOWN）
 * @param out    解析结果输出
 * @return 1 = 解析成功，0 = 帧不合法（不完整/CRC 错/地址不符）
 *
 * 注意：本函数不校验寄存器地址是否越界、数量是否合法，
 *       那属于"业务规则"，由上层（modbus_slave.c）判断。
 *       本函数只负责"这帧能不能读懂"。
 */
uint8_t ParseRequest(const uint8_t *frame, uint16_t len,
                     uint8_t my_addr, MbParseResult *out);

/*
 * 组 0x03 读保持寄存器的响应帧（不含寄存器数据，数据由调用方填充）。
 *
 * @param resp      输出缓冲区（至少 3 + qty*2 + 2 字节）
 * @param slave_addr 从机地址
 * @param qty       读取数量
 * @return 响应帧总字节数（= 3 + qty*2 + 2）
 */
uint16_t BuildReadHeader(uint8_t *resp, uint8_t slave_addr, uint8_t qty);

/*
 * 组 0x06 写单个寄存器的响应帧（原样回显请求）。
 * @param resp 输出缓冲区（至少 8 字节）
 * @param req  收到的完整请求帧
 * @param len  请求帧长度
 * @return 响应帧字节数（= len，写成功回显整帧）
 */
uint16_t BuildWriteResponse(uint8_t *resp, const uint8_t *req, uint16_t len);

/*
 * 组异常响应：[addr][func|0x80][excCode][crcLo][crcHi]
 * @param resp 输出缓冲区（至少 5 字节）
 * @return 5
 */
uint16_t BuildException(uint8_t *resp, uint8_t slave_addr, uint8_t func, uint8_t exc);

/*
 * 计算某段数据的 CRC 并追加到末尾（低字节在前）。
 * @param frame  缓冲区，前 len 字节是数据，第 len、len+1 字节写 CRC
 * @param len    数据长度
 * @return len + 2
 */
uint16_t AppendCrc(uint8_t *frame, uint16_t len);

#endif
