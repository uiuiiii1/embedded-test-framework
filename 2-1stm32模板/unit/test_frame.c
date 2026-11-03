/** @file    test_frame.c
 *  @brief   modbus_frame 纯逻辑的单元测试（PC 端，不依赖硬件）
 *
 *  编译：gcc test_frame.c unity/unity.c ../BSP/modbus_frame.c ../BSP/modbus_crc.c
 *  运行：./test_frame
 */

#include <stdint.h>
#include <string.h>

#include "unity.h"
#include "modbus_frame.h"
#include "modbus_crc.h"

void setUp(void) {}
void tearDown(void) {}


/* ============================================================
 * 一、ParseRequest 正常解析
 * ============================================================ */

void test_parse_read_ok(void)
{
	/* 0x03 读 4 个寄存器，CRC=44 09（低字节 44 在前） */
	uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x04, 0x44, 0x09};
	MbParseResult r;

	TEST_ASSERT_TRUE(ParseRequest(frame, 8, 1, &r));
	TEST_ASSERT_EQUAL(MB_FRAME_READ, r.type);
	TEST_ASSERT_EQUAL_UINT8(1, r.slave_addr);
	TEST_ASSERT_EQUAL_UINT8(0x03, r.func);
	TEST_ASSERT_EQUAL_UINT16(0, r.start);
	TEST_ASSERT_EQUAL_UINT16(4, r.qty);
}

void test_parse_read_offset(void)
{
	/* 读起始地址 0x1234，数量 2 */
	uint8_t frame[] = {0x01, 0x03, 0x12, 0x34, 0x00, 0x02, 0x00, 0x00};
	uint16_t crc = crc16_modbus(frame, 6);
	frame[6] = (uint8_t)(crc & 0xFF);
	frame[7] = (uint8_t)(crc >> 8);

	MbParseResult r;
	TEST_ASSERT_TRUE(ParseRequest(frame, 8, 1, &r));
	TEST_ASSERT_EQUAL_UINT16(0x1234, r.start);
	TEST_ASSERT_EQUAL_UINT16(2, r.qty);
}

void test_parse_write_ok(void)
{
	/* 0x06 写寄存器 5 = 0x1234 */
	uint8_t frame[] = {0x01, 0x06, 0x00, 0x05, 0x12, 0x34, 0x94, 0xBC};
	MbParseResult r;

	TEST_ASSERT_TRUE(ParseRequest(frame, 8, 1, &r));
	TEST_ASSERT_EQUAL(MB_FRAME_WRITE, r.type);
	TEST_ASSERT_EQUAL_UINT16(5, r.start);
	TEST_ASSERT_EQUAL_UINT16(0x1234, r.value);
}


/* ============================================================
 * 二、ParseRequest 异常输入
 * ============================================================ */

void test_parse_bad_crc_rejected(void)
{
	/* 最后 2 字节 CRC 改错 */
	uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x04, 0xFF, 0xFF};
	MbParseResult r;
	TEST_ASSERT_FALSE(ParseRequest(frame, 8, 1, &r));
}

void test_parse_too_short_rejected(void)
{
	uint8_t frame[] = {0x01, 0x03};
	MbParseResult r;
	TEST_ASSERT_FALSE(ParseRequest(frame, 2, 1, &r));
}

void test_parse_wrong_addr_rejected(void)
{
	/* 发给地址 2，本机是 1 */
	uint8_t frame[] = {0x02, 0x03, 0x00, 0x00, 0x00, 0x04, 0x44, 0x09};
	MbParseResult r;
	TEST_ASSERT_FALSE(ParseRequest(frame, 8, 1, &r));
}

void test_parse_null_pointer_rejected(void)
{
	MbParseResult r;
	TEST_ASSERT_FALSE(ParseRequest(NULL, 8, 1, &r));
	TEST_ASSERT_FALSE(ParseRequest((const uint8_t *)"x", 8, 1, NULL));
}

void test_parse_unknown_func_still_identifies(void)
{
	/* 功能码 0x07：能识别帧头，但 type=UNKNOWN，由上层决定回异常 */
	uint8_t frame[] = {0x01, 0x07, 0x00, 0x00, 0x00, 0x01, 0x75, 0xCA};
	MbParseResult r;
	TEST_ASSERT_TRUE(ParseRequest(frame, 8, 1, &r));
	TEST_ASSERT_EQUAL(MB_FRAME_UNKNOWN, r.type);
	TEST_ASSERT_EQUAL_UINT8(0x07, r.func);
}

void test_parse_wrong_len_read_rejected(void)
{
	/* 0x03 但只有 7 字节（缺 1 字节数据） */
	uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x04, 0x44};
	MbParseResult r;
	TEST_ASSERT_FALSE(ParseRequest(frame, 7, 1, &r));
}

void test_parse_read_wrong_len_9_rejected(void)
{
	/* 0x03 请求 9 字节：多了一字节，应拒绝 */
	uint8_t frame[9];
	uint16_t crc;
	memcpy(frame, (uint8_t[]){0x01, 0x03, 0x00, 0x00, 0x00, 0x04}, 6);
	crc = crc16_modbus(frame, 6);
	frame[6] = (uint8_t)(crc & 0xFF);
	frame[7] = (uint8_t)(crc >> 8);
	frame[8] = 0xAA;      /* 多出来的一字节 */

	MbParseResult r;
	TEST_ASSERT_FALSE(ParseRequest(frame, 9, 1, &r));
}

void test_parse_write_wrong_len_rejected(void)
{
	/* 0x06 请求 7 字节，应拒绝 */
	uint8_t frame[7];
	uint16_t crc;
	memcpy(frame, (uint8_t[]){0x01, 0x06, 0x00, 0x05, 0x12}, 5);
	crc = crc16_modbus(frame, 5);
	frame[5] = (uint8_t)(crc & 0xFF);
	frame[6] = (uint8_t)(crc >> 8);

	MbParseResult r;
	TEST_ASSERT_FALSE(ParseRequest(frame, 7, 1, &r));
}

void test_parse_read_wrong_len_6_rejected(void)
{
	/* 0x03 只有 6 字节（连 CRC 都不全），应拒绝 */
	uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x04};
	MbParseResult r;
	TEST_ASSERT_FALSE(ParseRequest(frame, 6, 1, &r));
}


/* ============================================================
 * 三、BuildReadHeader
 * ============================================================ */

void test_build_read_header_basic(void)
{
	uint8_t resp[16];
	uint16_t n = BuildReadHeader(resp, 1, 4);

	TEST_ASSERT_EQUAL_UINT16(3, n);              /* 只填了头部 */
	TEST_ASSERT_EQUAL_UINT8(1, resp[0]);
	TEST_ASSERT_EQUAL_UINT8(0x03, resp[1]);
	TEST_ASSERT_EQUAL_UINT8(8, resp[2]);         /* 4 个寄存器 * 2 = 8 字节 */
}

void test_build_read_header_qty1(void)
{
	uint8_t resp[16];
	uint16_t n = BuildReadHeader(resp, 1, 1);
	TEST_ASSERT_EQUAL_UINT8(2, resp[2]);         /* 1 * 2 = 2 */
	TEST_ASSERT_EQUAL_UINT16(3, n);
}


/* ============================================================
 * 四、BuildWriteResponse（写成功回显整帧）
 * ============================================================ */

void test_build_write_response_echo(void)
{
	uint8_t req[] = {0x01, 0x06, 0x00, 0x05, 0x12, 0x34, 0x94, 0xBC};
	uint8_t resp[16];
	uint16_t n = BuildWriteResponse(resp, req, 8);

	TEST_ASSERT_EQUAL_UINT16(8, n);
	TEST_ASSERT_EQUAL_UINT8_ARRAY(req, resp, 8);
}


/* ============================================================
 * 五、BuildException
 * ============================================================ */

void test_build_exception_illegal_function(void)
{
	uint8_t resp[8];
	uint16_t n = BuildException(resp, 1, 0x07, 0x01);

	TEST_ASSERT_EQUAL_UINT16(5, n);
	TEST_ASSERT_EQUAL_UINT8(1, resp[0]);
	TEST_ASSERT_EQUAL_UINT8(0x87, resp[1]);      /* 0x07 | 0x80 */
	TEST_ASSERT_EQUAL_UINT8(0x01, resp[2]);      /* 异常码 */
	TEST_ASSERT_TRUE(crc16_modbus(resp, 3) == (uint16_t)(resp[3] | (resp[4] << 8)));
}

void test_build_exception_illegal_address(void)
{
	uint8_t resp[8];
	uint16_t n = BuildException(resp, 1, 0x03, 0x02);
	TEST_ASSERT_EQUAL_UINT16(5, n);
	TEST_ASSERT_EQUAL_UINT8(0x83, resp[1]);
	TEST_ASSERT_EQUAL_UINT8(0x02, resp[2]);
}

void test_build_exception_illegal_value(void)
{
	uint8_t resp[8];
	uint16_t n = BuildException(resp, 1, 0x06, 0x03);
	TEST_ASSERT_EQUAL_UINT16(5, n);
	TEST_ASSERT_EQUAL_UINT8(0x86, resp[1]);
	TEST_ASSERT_EQUAL_UINT8(0x03, resp[2]);
}


/* ============================================================
 * 六、AppendCrc
 * ============================================================ */

void test_append_crc_little_endian(void)
{
	uint8_t frame[8] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00};
	uint16_t n = AppendCrc(frame, 6);

	TEST_ASSERT_EQUAL_UINT16(8, n);
	TEST_ASSERT_EQUAL_UINT8(0xC5, frame[6]);     /* 低字节 0xC5 */
	TEST_ASSERT_EQUAL_UINT8(0xCD, frame[7]);     /* 高字节 0xCD */
}


/* ============================================================
 * 入口
 * ============================================================ */

int main(void)
{
	UNITY_BEGIN();

	RUN_TEST(test_parse_read_ok);
	RUN_TEST(test_parse_read_offset);
	RUN_TEST(test_parse_write_ok);

	RUN_TEST(test_parse_bad_crc_rejected);
	RUN_TEST(test_parse_too_short_rejected);
	RUN_TEST(test_parse_wrong_addr_rejected);
	RUN_TEST(test_parse_null_pointer_rejected);
	RUN_TEST(test_parse_unknown_func_still_identifies);
	RUN_TEST(test_parse_wrong_len_read_rejected);
	RUN_TEST(test_parse_read_wrong_len_9_rejected);
	RUN_TEST(test_parse_write_wrong_len_rejected);
	RUN_TEST(test_parse_read_wrong_len_6_rejected);

	RUN_TEST(test_build_read_header_basic);
	RUN_TEST(test_build_read_header_qty1);

	RUN_TEST(test_build_write_response_echo);

	RUN_TEST(test_build_exception_illegal_function);
	RUN_TEST(test_build_exception_illegal_address);
	RUN_TEST(test_build_exception_illegal_value);

	RUN_TEST(test_append_crc_little_endian);

	return UNITY_END();
}
