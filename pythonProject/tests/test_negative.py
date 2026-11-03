"""异常注入用例：主动构造非法报文，验证从机的错误处理。

这些是最能体现"测试开发能力"的用例——不是等 bug 来，而是主动构造异常。
"""

import pytest

from framework import modbus

pytestmark = [pytest.mark.regression, pytest.mark.negative]


class TestCrcInjection:
    """CRC 异常注入。"""

    def test_bad_crc_is_dropped(self, dut):
        """CRC 篡改后从机应静默丢弃，不应答。"""
        frame = bytearray(modbus.build_read_holding(1, 0, 1))
        frame[-1] ^= 0xFF           # 篡改 CRC 高字节
        dut.send_raw(bytes(frame))
        assert dut.read_silent() == b"", "CRC 错误的帧不应收到任何响应"

    def test_bad_crc_write_is_dropped(self, dut):
        """CRC 错误的写请求不能生效。"""
        before = dut.read_regs(7, 1)

        frame = bytearray(modbus.build_write_single(1, 7, 0xDEAD))
        frame[4] ^= 0x01            # 篡改数据字节，破坏 CRC
        dut.send_raw(bytes(frame))
        assert dut.read_silent() == b"", "CRC 错误的写帧不应有响应"

        assert dut.read_regs(7, 1) == before, "CRC 错误的写请求不应改变寄存器"

    def test_truncated_frame_is_dropped(self, dut):
        """只发半帧（缺 CRC），从机应丢弃。"""
        frame = modbus.build_read_holding(1, 0, 1)[:5]
        dut.send_raw(frame)
        assert dut.read_silent() == b""


class TestIllegalFunction:
    """非法功能码注入。"""

    @pytest.mark.parametrize("func", [0x01, 0x02, 0x04, 0x05, 0x07, 0x0F, 0x10, 0x2B])
    def test_unsupported_function(self, dut, func):
        """未实现的功能码 -> 异常 0x01，且响应功能码 = 原码 | 0x80。"""
        body = bytes([0x01, func, 0x00, 0x00, 0x00, 0x01])
        frame = modbus.append_crc(body)

        raw = dut.read_raw(frame, expect_len=5)
        parsed = modbus.parse_response(raw)

        assert parsed["exception"] == 0x01, f"功能码 0x{func:02X} 应返回异常 0x01"
        assert parsed["func"] == func
        assert raw[1] == (func | 0x80), "响应功能码最高位应为 1"


class TestIllegalAddress:
    """非法寄存器地址注入。"""

    @pytest.mark.parametrize("start", [100, 101, 200, 1000, 0xFFFF])
    def test_read_illegal_address(self, dut, start):
        raw = dut.read_raw(modbus.build_read_holding(1, start, 1), expect_len=5)
        parsed = modbus.parse_response(raw)
        assert parsed["exception"] == 0x02

    @pytest.mark.parametrize("reg", [100, 500, 0xFFFF])
    def test_write_illegal_address(self, dut, reg):
        raw = dut.read_raw(modbus.build_write_single(1, reg, 0x1111), expect_len=5)
        parsed = modbus.parse_response(raw)
        assert parsed["exception"] == 0x02


class TestIllegalQty:
    """非法数量注入。"""

    @pytest.mark.parametrize("qty", [0, 126, 127, 255, 0xFFFF])
    def test_illegal_qty(self, dut, qty):
        raw = dut.read_raw(modbus.build_read_holding(1, 0, qty), expect_len=5)
        parsed = modbus.parse_response(raw)
        assert parsed["exception"] == 0x03


class TestWrongSlaveAddress:
    """从机地址错误：不该应答。"""

    @pytest.mark.parametrize("addr", [2, 5, 0xFF])
    def test_wrong_address_no_response(self, dut, addr):
        """发给其他地址，本机应保持沉默。"""
        frame = modbus.build_read_holding(addr, 0, 1)
        dut.send_raw(frame)
        assert dut.read_silent() == b"", f"地址 {addr} 的帧不应被本机应答"
