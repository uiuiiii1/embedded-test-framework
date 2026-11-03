"""健壮性 / 时序用例：帧间隔、粘包、超时。

这些是嵌入式测试区别于普通软件测试的地方——
要验证的不是"数据对不对"，而是"时序行为对不对"。
"""

import time

import pytest

from framework import modbus

pytestmark = [pytest.mark.regression]

# 9600 波特率，每字符 11 bit -> 1 字符 ≈ 1.146ms -> 3.5 字符 ≈ 4.0ms
T35_SECONDS = 4.0e-3


class TestFrameGap:
    """帧间隔（3.5 字符时间）。"""

    def test_two_frames_with_gap_both_answered(self, dut):
        """两帧间隔 >= 3.5 字符时间，两帧都应正常响应。"""
        f1 = modbus.build_read_holding(1, 0, 1)
        f2 = modbus.build_read_holding(1, 1, 1)

        dut.send_raw(f1)
        time.sleep(T35_SECONDS * 3)          # 12ms，足够
        resp1 = dut.transport.recv_exact(7, timeout=0.5)
        assert modbus.parse_response(resp1)["data"] is not None

        dut.send_raw(f2)
        resp2 = dut.transport.recv_exact(7, timeout=0.5)
        assert modbus.parse_response(resp2)["data"] is not None

    def test_sticky_frames_are_merged(self, dut):
        """两帧粘在一起发（无间隔）：
        从机应按 3.5 字符规则判定为一帧，CRC 校验失败后整帧丢弃。
        期望：无响应。
        """
        f1 = modbus.build_read_holding(1, 0, 1)
        f2 = modbus.build_read_holding(1, 1, 1)

        # 一次 write 写出 16 字节，中间零间隔
        dut.send_raw(f1 + f2)
        assert dut.read_silent(timeout=0.3) == b"", (
            "粘包帧不应被拆成两帧处理；应按一帧解析并因 CRC 失败丢弃"
        )


class TestTimeout:
    """响应超时。"""

    def test_timeout_does_not_crash_framework(self, dut):
        """发一帧合帧后，等不到响应时会抛 TransportTimeout，框架不崩。"""
        from framework.transport import TransportTimeout

        dut.send_raw(b"\xAA")                # 垃圾字节，从机不会回
        with pytest.raises(TransportTimeout):
            dut.transport.recv_exact(1, timeout=0.2)

    def test_recover_after_timeout(self, dut):
        """超时之后重新发正常帧，应能恢复正常。"""
        dut.send_raw(b"\xAA")
        with pytest.raises(Exception):
            dut.transport.recv_exact(1, timeout=0.2)

        # 清掉可能的残留，再正常通信
        dut.transport.reset_input_buffer()
        assert dut.read_regs(0, 1) is not None


class TestFrameRecovery:
    """异常帧之后的恢复能力。"""

    def test_recover_after_bad_crc(self, dut):
        """发一个 CRC 错误的帧后再发正常帧，应恢复正常。"""
        bad = bytearray(modbus.build_read_holding(1, 0, 1))
        bad[-1] ^= 0xFF
        dut.send_raw(bytes(bad))
        dut.read_silent(timeout=0.2)

        assert dut.read_regs(0, 1) is not None

    def test_recover_after_garbage(self, dut):
        """发一堆垃圾字节后再发正常帧，应能恢复。"""
        dut.send_raw(b"\xAA\xBB\xCC\xDD")
        dut.read_silent(timeout=0.2)

        assert dut.read_regs(0, 1) is not None
