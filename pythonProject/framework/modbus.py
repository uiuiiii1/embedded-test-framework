"""Modbus RTU 协议层：只负责字节的组装与解析，不关心字节怎么发出去。"""

from __future__ import annotations


def crc16_modbus(data: bytes) -> int:
    """Modbus RTU 标准 CRC16，多项式 0xA001，初值 0xFFFF。"""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def append_crc(frame: bytes) -> bytes:
    """在帧尾追加 CRC，低字节在前。"""
    crc = crc16_modbus(frame)
    return frame + bytes([crc & 0xFF, (crc >> 8) & 0xFF])


def check_crc(frame: bytes) -> bool:
    """校验整帧（含末尾 2 字节 CRC）是否正确。"""
    if len(frame) < 2:
        return False
    expected = crc16_modbus(frame[:-2])
    received = frame[-2] | (frame[-1] << 8)
    return expected == received


def build_read_holding(slave_addr: int, start: int, qty: int) -> bytes:
    """组 0x03 读保持寄存器的请求帧（8 字节，已含 CRC）。"""
    body = bytes([
        slave_addr & 0xFF,
        0x03,
        (start >> 8) & 0xFF, start & 0xFF,
        (qty >> 8) & 0xFF, qty & 0xFF,
    ])
    return append_crc(body)


def build_write_single(slave_addr: int, reg: int, value: int) -> bytes:
    """组 0x06 写单个寄存器的请求帧（8 字节，已含 CRC）。"""
    body = bytes([
        slave_addr & 0xFF,
        0x06,
        (reg >> 8) & 0xFF, reg & 0xFF,
        (value >> 8) & 0xFF, value & 0xFF,
    ])
    return append_crc(body)


def parse_response(frame: bytes) -> dict:
    """
    解析从机响应帧。

    正常响应返回：
        {'addr': int, 'func': int, 'data': list[int]}   # 0x03
        {'addr': int, 'func': int, 'value': int}        # 0x06
    异常响应返回：
        {'addr': int, 'func': int, 'exception': int}

    CRC 错误会抛 ValueError。
    """
    if len(frame) < 4:
        raise ValueError(f"响应帧太短: {frame.hex(' ')}")
    if not check_crc(frame):
        raise ValueError(f"响应帧 CRC 校验失败: {frame.hex(' ')}")

    addr = frame[0]
    func = frame[1]

    # 异常响应：[addr][func|0x80][exc][crcLo][crcHi]
    if func & 0x80:
        return {"addr": addr, "func": func & 0x7F, "exception": frame[2]}

    if func == 0x03:
        byte_count = frame[2]
        payload = frame[3:3 + byte_count]
        values = [(payload[i] << 8) | payload[i + 1] for i in range(0, byte_count, 2)]
        return {"addr": addr, "func": func, "data": values}

    if func == 0x06:
        value = (frame[4] << 8) | frame[5]
        return {"addr": addr, "func": func, "value": value}

    raise ValueError(f"不支持的功能码: 0x{func:02X}")
