"""缺陷回归用例：每个在 defects.md 里登记的缺陷，对应一条用例。

命名规则：test_reg_bugNNN_xxx
这些用例的价值是"防止同一个 BUG 再次出现"——
将来任何人改了 modbus_slave.c，只要重新引入这个缺陷，这里就会立刻报红。
"""

import time

import pytest

from framework import modbus

pytestmark = [pytest.mark.regression]


class TestBug001PartialFrameRecovery:
    """BUG-001：收到残缺帧后从机永久失联。

    缺陷现象：
        发送一帧不完整的报文（例如只有 5 字节，缺 CRC）后，
        从机此后不再响应任何合法请求，必须复位才能恢复。

    根因：
        接收缓冲长度只在"攒够整帧"时清零。
        残缺帧留下的 1~7 字节永远残留在缓冲里，
        导致后续所有帧的帧头错位，CRC 永远校验失败。

    修复：
        改用空闲超时（3.5 字符）断帧，并加接收超时兜底清空缓冲。
    """

    def test_reg_bug001_partial_frame_recovery(self, dut):
        """发送残缺帧后，从机必须仍能正常响应。"""
        # 1. 基线：先确认设备正常工作
        assert dut.read_regs(0, 1) is not None

        # 2. 注入残缺帧（5 字节，缺数量和 CRC）
        dut.send_raw(bytes.fromhex("0103000000"))
        dut.read_silent(timeout=0.2)

        # 3. 关键断言：设备必须能恢复
        assert dut.read_regs(0, 1) is not None, (
            "BUG-001 复发：发送残缺帧后，从机失去响应"
        )

    @pytest.mark.parametrize("truncated_len", [1, 2, 3, 4, 5, 6, 7])
    def test_reg_bug001_various_truncated_lengths(self, dut, truncated_len):
        """各种长度的残缺帧都不能导致死锁。"""
        full = modbus.build_read_holding(1, 0, 1)
        dut.send_raw(full[:truncated_len])
        dut.read_silent(timeout=0.1)

        assert dut.read_regs(0, 1) is not None, (
            f"BUG-001 复发：{truncated_len} 字节残缺帧导致从机死锁"
        )

    def test_reg_bug001_after_idle_timeout(self, dut):
        """残缺帧后等待足够长的空闲时间（超过接收超时），必须能恢复。"""
        dut.send_raw(bytes.fromhex("010300"))
        time.sleep(0.1)              # 超过 MODBUS_RX_TIMEOUT_MS(50ms)

        assert dut.read_regs(0, 1) is not None


class TestBug002StickyFrames:
    """BUG-002：粘包（两帧零间隔连发）处理。

    标准要求：从机应按 3.5 字符空闲判定帧边界。
    两帧零间隔连发应被当作一帧，CRC 失败后丢弃，且不能影响后续通信。
    """

    def test_reg_bug002_sticky_frames_no_side_effect(self, dut):
        """粘包帧被丢弃后，设备必须能继续正常工作。"""
        f1 = modbus.build_read_holding(1, 0, 1)
        f2 = modbus.build_read_holding(1, 1, 1)

        # 一次性写出两帧，无间隔
        dut.send_raw(f1 + f2)
        dut.read_silent(timeout=0.2)

        # 关键：不能残留状态
        assert dut.read_regs(0, 1) is not None, (
            "BUG-002 复发：粘包处理后从机无法继续工作"
        )

    def test_reg_bug002_three_frames_sticky(self, dut):
        """三帧连发同样不能造成状态残留。"""
        frames = b"".join(
            modbus.build_read_holding(1, i, 1) for i in range(3)
        )
        dut.send_raw(frames)
        dut.read_silent(timeout=0.2)

        assert dut.read_regs(0, 1) is not None
