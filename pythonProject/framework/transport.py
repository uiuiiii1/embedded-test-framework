"""传输层：只负责字节的进出，不关心内容。

SerialTransport  -> 真实串口
LoopbackTransport -> 内存回环（后面接模拟器用）
"""

from __future__ import annotations

import time
from abc import ABC, abstractmethod


class TransportError(Exception):
    """传输层异常基类。"""


class TransportTimeout(TransportError):
    """读取超时。"""


class TransportClosed(TransportError):
    """端口未打开或被关闭。"""


class Transport(ABC):
    """所有传输方式的统一接口。"""

    @abstractmethod
    def send(self, data: bytes) -> None:
        ...

    @abstractmethod
    def recv_exact(self, n: int, timeout: float = 0.5) -> bytes:
        """按期望字节数精确读取，读不满则抛 TransportTimeout。"""

    @abstractmethod
    def close(self) -> None:
        ...


class SerialTransport(Transport):
    """真实串口。"""

    def __init__(self, port: str, baudrate: int = 9600, timeout: float = 0.5):
        try:
            import serial
        except ImportError as exc:  # pragma: no cover
            raise TransportError("缺少 pyserial，请执行 pip install pyserial") from exc

        self._inter_frame_gap = 0.0
        try:
            self._ser = serial.Serial(
                port=port,
                baudrate=baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=timeout,
            )
            # 打开后清掉残留数据，避免上一轮测试的字节污染本次会话
            self._ser.reset_input_buffer()
            self._ser.reset_output_buffer()
        except serial.SerialException as exc:
            raise TransportError(
                f"串口 {port} 打开失败，请检查：\n"
                f"  1) 串口是否被串口助手占用（先关掉串口助手）\n"
                f"  2) USB-TTL 是否插好\n"
                f"  3) 端口号是否正确\n"
                f"原始错误：{exc}"
            ) from exc

    def send(self, data: bytes) -> None:
        if self._inter_frame_gap > 0:
            time.sleep(self._inter_frame_gap)
        self._ser.write(data)
        self._ser.flush()

    def recv_exact(self, n: int, timeout: float = 0.5) -> bytes:
        deadline = time.monotonic() + timeout
        buf = b""
        while len(buf) < n:
            chunk = self._ser.read(n - len(buf))
            if chunk:
                buf += chunk
            if time.monotonic() > deadline:
                raise TransportTimeout(
                    f"等待 {n} 字节超时，只收到 {len(buf)} 字节: {buf.hex(' ')}"
                )
        return buf

    def set_inter_frame_gap(self, gap_s: float) -> None:
        """发送两帧之间的间隔，用于验证 3.5 字符间隔。"""
        self._inter_frame_gap = gap_s

    def reset_input_buffer(self) -> None:
        self._ser.reset_input_buffer()

    def close(self) -> None:
        if getattr(self, "_ser", None) is not None and self._ser.is_open:
            self._ser.close()


class LoopbackTransport(Transport):
    """把字节直接交给内存里的从机模拟器，不经过串口。"""

    def __init__(self, slave):
        self._slave = slave
        self._out = bytearray()

    def send(self, data: bytes) -> None:
        self._out += self._slave.handle_request(bytes(data))

    def recv_exact(self, n: int, timeout: float = 0.5) -> bytes:
        if len(self._out) < n:
            raise TransportTimeout(
                f"等待 {n} 字节超时，只收到 {len(self._out)} 字节: {bytes(self._out).hex(' ')}"
            )
        data = bytes(self._out[:n])
        del self._out[:n]
        return data

    def close(self) -> None:
        pass
