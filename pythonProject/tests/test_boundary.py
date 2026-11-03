"""边界值用例：数量、地址的上下限。

固件约定（modbus_slave.c）：
- 100 个寄存器，编号 0..99
- 0x03 一次最多读 125 个，但实际受寄存器总数限制
- 越界地址 -> 异常 0x02
- 数量 0 或 >125 -> 异常 0x03
"""

import pytest

from framework import modbus
from framework.dut import ModbusError

pytestmark = [pytest.mark.regression, pytest.mark.boundary]

REG_COUNT = 100
MAX_READ_QTY = 125


class TestReadQtyBoundary:
    """读取数量边界。"""

    def test_qty_zero_is_exception(self, dut):
        """数量 = 0 -> 异常 0x03（非法数据值）。"""
        raw = dut.read_raw(modbus.build_read_holding(1, 0, 0), expect_len=5)
        parsed = modbus.parse_response(raw)
        assert parsed.get("exception") == 0x03

    def test_qty_one_ok(self, dut):
        """数量 = 1 -> 正常返回 1 个（最小值）。"""
        assert len(dut.read_regs(0, 1)) == 1

    def test_qty_126_is_exception(self, dut):
        """数量 = 126 -> 异常 0x03（超上限）。"""
        raw = dut.read_raw(modbus.build_read_holding(1, 0, 126), expect_len=5)
        parsed = modbus.parse_response(raw)
        assert parsed.get("exception") == 0x03

    def test_qty_2000_is_exception(self, dut):
        """数量 = 2000 -> 异常 0x03。"""
        raw = dut.read_raw(modbus.build_read_holding(1, 0, 2000), expect_len=5)
        parsed = modbus.parse_response(raw)
        assert parsed.get("exception") == 0x03

    def test_qty_125_is_legal_value(self, dut):
        """数量 = 125 协议上合法，但起始 0 + 125 会越过 100 个寄存器，
        所以这里期望异常 0x02（地址越界）而不是 0x03。"""
        raw = dut.read_raw(modbus.build_read_holding(1, 0, 125), expect_len=5)
        parsed = modbus.parse_response(raw)
        assert parsed.get("exception") == 0x02

    def test_qty_100_full_range_ok(self, dut):
        """读满全部 100 个寄存器应成功。"""
        values = dut.read_regs(0, 100)
        assert len(values) == 100


class TestReadAddressBoundary:
    """读取地址边界。"""

    def test_start_zero_ok(self, dut):
        """起始地址 0（下边界）正常。"""
        assert len(dut.read_regs(0, 1)) == 1

    def test_start_last_register_ok(self, dut):
        """起始地址 99（最后一个寄存器）正常。"""
        assert len(dut.read_regs(REG_COUNT - 1, 1)) == 1

    def test_start_out_of_range_is_exception(self, dut):
        """起始地址 100（越界）-> 异常 0x02。"""
        raw = dut.read_raw(modbus.build_read_holding(1, 100, 1), expect_len=5)
        parsed = modbus.parse_response(raw)
        assert parsed.get("exception") == 0x02

    def test_start_0xffff_is_exception(self, dut):
        """起始地址 0xFFFF -> 异常 0x02。"""
        raw = dut.read_raw(modbus.build_read_holding(1, 0xFFFF, 1), expect_len=5)
        parsed = modbus.parse_response(raw)
        assert parsed.get("exception") == 0x02

    def test_range_straddle_is_exception(self, dut):
        """起始 99 + 数量 2 会跨出边界 -> 异常 0x02。"""
        raw = dut.read_raw(modbus.build_read_holding(1, 99, 2), expect_len=5)
        parsed = modbus.parse_response(raw)
        assert parsed.get("exception") == 0x02


class TestWriteBoundary:
    """写入边界。"""

    @pytest.mark.parametrize("value", [0x0000, 0x0001, 0x7FFF, 0x8000, 0xFFFE, 0xFFFF])
    def test_write_values(self, dut, value):
        """各种边界值写入后回读一致。"""
        dut.write_reg(30, value)
        assert dut.read_regs(30, 1) == [value]

    def test_write_last_register_ok(self, dut):
        """写最后一个寄存器（99）正常。"""
        dut.write_reg(REG_COUNT - 1, 0xBEEF)
        assert dut.read_regs(REG_COUNT - 1, 1) == [0xBEEF]

    def test_write_out_of_range_is_exception(self, dut):
        """写越界寄存器 100 -> 异常 0x02。"""
        raw = dut.read_raw(modbus.build_write_single(1, 100, 0x1234), expect_len=5)
        parsed = modbus.parse_response(raw)
        assert parsed.get("exception") == 0x02
