"""DUT 层：被测设备的业务动作。只关心"对从机做什么"，不关心串口细节。"""

from __future__ import annotations

from . import modbus
from .transport import Transport, TransportTimeout


class ModbusError(Exception):
    """从机返回异常响应。"""

    def __init__(self, func: int, code: int):
        self.func = func
        self.code = code
        super().__init__(f"从机异常响应：功能码 0x{func:02X}，异常码 0x{code:02X}")


class ModbusDUT:
    """一个 Modbus RTU 从机的客户端封装。"""

    def __init__(self, transport: Transport, slave_addr: int = 1, timeout: float = 0.5):
        self.transport = transport
        self.slave_addr = slave_addr
        self.timeout = timeout

    # ---------- 正常功能 ----------

    def read_regs(self, start: int, qty: int) -> list[int]:
        """0x03 读保持寄存器，返回寄存器值列表。"""
        frame = modbus.build_read_holding(self.slave_addr, start, qty)
        resp = self.read_raw(frame, expect_len=5 + qty * 2)
        return self._unwrap(resp)["data"]

    def write_reg(self, reg: int, value: int) -> None:
        """0x06 写单个寄存器，成功后返回 None。"""
        frame = modbus.build_write_single(self.slave_addr, reg, value)
        resp = self.read_raw(frame, expect_len=8)
        self._unwrap(resp)

    # ---------- 底层 ----------

    def read_raw(self, frame: bytes, expect_len: int) -> bytes:
        """发原始帧、按期望长度返回原始响应。异常注入测试用这个。

        expect_len 必须由调用方显式计算，不给默认值——
        读 1 个寄存器响应是 7 字节，读 10 个是 25 字节，
        给个默认 8 只会制造"静默读错长度"的陷阱。
        """
        self.transport.send(frame)
        return self.transport.recv_exact(expect_len, timeout=self.timeout)

    def read_raw_auto(self, frame: bytes, qty: int) -> bytes:
        """0x03 读请求的便捷版本：按 qty 自动算出响应长度。"""
        return self.read_raw(frame, expect_len=5 + qty * 2)

    def send_raw(self, frame: bytes) -> None:
        """只发不读，用于验证"应该没有响应"的情况。"""
        self.transport.send(frame)

    def read_silent(self, timeout: float = 0.3) -> bytes:
        """尝试读取，期望没有响应；有响应则返回收到的字节，无响应返回 b""。"""
        try:
            return self.transport.recv_exact(1, timeout=timeout)
        except TransportTimeout:
            return b""

    def _unwrap(self, resp: bytes) -> dict:
        """解析响应，若为异常响应则抛 ModbusError。"""
        parsed = modbus.parse_response(resp)
        if "exception" in parsed:
            raise ModbusError(parsed["func"], parsed["exception"])
        return parsed

    def close(self) -> None:
        """DUT 层不拥有 transport，关闭的责任在创建 transport 的一方（fixture）。

        注意：这里故意不调用 self.transport.close()，因为 Ser 是 session 级
        共享句柄，若每条用例都关一次，后续用例就会拿到已关闭的端口。
        """
        pass
