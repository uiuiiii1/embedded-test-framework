"""正向功能用例：0x03 读、0x06 写。

固件约定（与 modbus_slave.c 一致）：
- 从机地址 1
- 100 个保持寄存器，上电初值 regs[i] = i
- 寄存器会被其他用例改写，所以这里不依赖初值，只验证读写一致性
"""

import pytest

from framework import modbus

pytestmark = [pytest.mark.smoke]


class TestReadHolding:
    def test_read_one(self, dut):
        """读 1 个寄存器，返回 1 个值且 CRC 正确。"""
        raw = dut.read_raw_auto(modbus.build_read_holding(dut.slave_addr, 0, 1), qty=1)
        assert modbus.check_crc(raw), f"响应 CRC 错误: {raw.hex(' ')}"

        parsed = modbus.parse_response(raw)
        assert parsed["addr"] == dut.slave_addr
        assert parsed["func"] == 0x03
        assert len(parsed["data"]) == 1

    def test_read_ten(self, dut):
        """读连续 10 个寄存器。"""
        values = dut.read_regs(10, 10)
        assert len(values) == 10

    @pytest.mark.parametrize("qty", [1, 2, 8, 10, 50])
    def test_read_various_qty(self, dut, qty):
        """不同读取数量都能正确返回对应个数。"""
        values = dut.read_regs(0, qty)
        assert len(values) == qty


class TestWriteSingle:
    def test_write_then_read_back(self, dut):
        """写单个寄存器后回读，值必须一致 —— 最核心的验证方法。"""
        dut.write_reg(5, 0x1234)
        assert dut.read_regs(5, 1) == [0x1234]

    @pytest.mark.parametrize("value", [0x0000, 0xFFFF, 0x00FF, 0xFF00])
    def test_write_boundary_values(self, dut, value):
        """写边界值 0x0000 / 0xFFFF 后回读一致。"""
        dut.write_reg(10, value)
        assert dut.read_regs(10, 1) == [value]

    def test_write_does_not_affect_neighbors(self, dut):
        """写一个寄存器不应影响相邻寄存器。"""
        dut.write_reg(20, 0xAAAA)
        dut.write_reg(21, 0xBBBB)
        dut.write_reg(22, 0xCCCC)

        # 只改中间那个
        dut.write_reg(21, 0x0000)

        assert dut.read_regs(20, 3) == [0xAAAA, 0x0000, 0xCCCC]
