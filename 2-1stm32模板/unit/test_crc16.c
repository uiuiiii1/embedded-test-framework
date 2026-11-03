/**
 * @file    test_crc16.c
 * @brief   crc16_modbus() 的单元测试（Unity 框架）
 *
 * 被测代码：BSP/modbus_crc.c（纯算法，无硬件依赖）
 * 编译方式：见 Makefile / run.bat
 */

#include <stdint.h>
#include <string.h>

#include "unity.h"
#include "modbus_crc.h"

void setUp(void)
{
}

void tearDown(void)
{
}


/* ============================================================
 * 一、标准测试向量（Modbus 规范里的已知答案）
 * ============================================================ */

void test_crc_modbus_standard_vector(void)
{
	/* 这是 Modbus RTU 最经典的一条：读 10 个保持寄存器 */
	uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
	TEST_ASSERT_EQUAL_HEX16(0xCDC5, crc16_modbus(frame, 6));
}

void test_crc_single_zero_byte(void)
{
	uint8_t b = 0x00;
	TEST_ASSERT_EQUAL_HEX16(0x40BF, crc16_modbus(&b, 1));
}

void test_crc_empty_returns_initial_value(void)
{
	/* 长度为 0 时 CRC 应保持初值 0xFFFF */
	TEST_ASSERT_EQUAL_HEX16(0xFFFF, crc16_modbus(NULL, 0));
}

void test_crc_null_pointer_returns_initial_value(void)
{
	/* 空指针 + 长度 0 也是合法调用 */
	TEST_ASSERT_EQUAL_HEX16(0xFFFF, crc16_modbus(NULL, 0));
}


/* ============================================================
 * 二、与 Python 实现交叉验证的向量
 *    （框架 framework/modbus.py 算出来的结果，两套实现互相对照）
 * ============================================================ */

void test_crc_read_one_register(void)
{
	uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01};
	TEST_ASSERT_EQUAL_HEX16(0x0A84, crc16_modbus(frame, 6));
}

void test_crc_read_four_registers(void)
{
	uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x04};
	TEST_ASSERT_EQUAL_HEX16(0x0944, crc16_modbus(frame, 6));
}

void test_crc_write_single(void)
{
	uint8_t frame[] = {0x01, 0x06, 0x00, 0x05, 0x12, 0x34};
	TEST_ASSERT_EQUAL_HEX16(0xBC94, crc16_modbus(frame, 6));
}

void test_crc_read_ten_registers(void)
{
	uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
	TEST_ASSERT_EQUAL_HEX16(0xCDC5, crc16_modbus(frame, 6));
}


/* ============================================================
 * 三、性质测试（property-based 思路）
 * ============================================================ */

void test_crc_is_deterministic(void)
{
	uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x64};
	uint16_t a = crc16_modbus(frame, 6);
	uint16_t b = crc16_modbus(frame, 6);
	TEST_ASSERT_EQUAL_HEX16(a, b);
}

void test_crc_differs_when_one_byte_changes(void)
{
	uint8_t f1[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01};
	uint8_t f2[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02};

	TEST_ASSERT_NOT_EQUAL(
		crc16_modbus(f1, 6),
		crc16_modbus(f2, 6)
	);
}

void test_crc_all_single_bytes_are_unique(void)
{
	/* 单字节 0x00~0xFF 的 CRC 两两不同（简单查重，覆盖面广） */
	uint16_t seen[256];
	int i, j;

	for (i = 0; i < 256; i++)
	{
		uint8_t b = (uint8_t)i;
		seen[i] = crc16_modbus(&b, 1);
	}

	for (i = 0; i < 256; i++)
	{
		for (j = i + 1; j < 256; j++)
		{
			TEST_ASSERT_NOT_EQUAL_MESSAGE(seen[i], seen[j], "单字节 CRC 出现碰撞");
		}
	}
}

void test_crc_covers_all_byte_values(void)
{
	/* 遍历全部 256 个字节值，确保算法分支都被走一遍 */
	int i;
	for (i = 0; i < 256; i++)
	{
		uint8_t b = (uint8_t)i;
		(void)crc16_modbus(&b, 1);
	}
	TEST_PASS();
}


/* ============================================================
 * 四、边界与异常输入
 * ============================================================ */

void test_crc_length_zero_nonnull(void)
{
	uint8_t buf[4] = {0x11, 0x22, 0x33, 0x44};
	/* 长度为 0 时不应读取任何字节 */
	TEST_ASSERT_EQUAL_HEX16(0xFFFF, crc16_modbus(buf, 0));
}

void test_crc_length_one(void)
{
	uint8_t buf[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
	/* 只算第一个字节，后面的不应影响结果 */
	uint8_t expected_buf = 0xAA;
	TEST_ASSERT_EQUAL_HEX16(
		crc16_modbus(&expected_buf, 1),
		crc16_modbus(buf, 1)
	);
}

void test_crc_does_not_modify_input(void)
{
	uint8_t original[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
	uint8_t copy[6];

	memcpy(copy, original, sizeof(original));
	(void)crc16_modbus(copy, 6);

	TEST_ASSERT_EQUAL_UINT8_ARRAY(original, copy, 6);
}

void test_crc_large_frame(void)
{
	/* 256 字节的长帧（Modbus RTU 帧长上限） */
	uint8_t big[256];
	int i;
	for (i = 0; i < 256; i++)
	{
		big[i] = (uint8_t)i;
	}
	/* 只验证不崩溃、结果稳定 */
	uint16_t a = crc16_modbus(big, 256);
	uint16_t b = crc16_modbus(big, 256);
	TEST_ASSERT_EQUAL_HEX16(a, b);
}


/* ============================================================
 * 入口
 * ============================================================ */

int main(void)
{
	UNITY_BEGIN();

	RUN_TEST(test_crc_modbus_standard_vector);
	RUN_TEST(test_crc_single_zero_byte);
	RUN_TEST(test_crc_empty_returns_initial_value);
	RUN_TEST(test_crc_null_pointer_returns_initial_value);

	RUN_TEST(test_crc_read_one_register);
	RUN_TEST(test_crc_read_four_registers);
	RUN_TEST(test_crc_write_single);
	RUN_TEST(test_crc_read_ten_registers);

	RUN_TEST(test_crc_is_deterministic);
	RUN_TEST(test_crc_differs_when_one_byte_changes);
	RUN_TEST(test_crc_all_single_bytes_are_unique);
	RUN_TEST(test_crc_covers_all_byte_values);

	RUN_TEST(test_crc_length_zero_nonnull);
	RUN_TEST(test_crc_length_one);
	RUN_TEST(test_crc_does_not_modify_input);
	RUN_TEST(test_crc_large_frame);

	return UNITY_END();
}
