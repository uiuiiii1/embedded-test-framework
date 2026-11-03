"""pytest 全局夹具：串口生命周期管理。

串口是独占资源，所以 scope="session"（整个测试会话共用一个句柄）。
DUT 是功能级，每条用例拿一个新 DUT。

切到模拟器：设置环境变量 MODBUS_USE_SIM=1
"""

import os
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from framework.dut import ModbusDUT
from framework.transport import SerialTransport, TransportError

DEFAULT_PORT = os.environ.get("MODBUS_PORT", "COM3")
BAUDRATE = int(os.environ.get("MODBUS_BAUDRATE", "9600"))


@pytest.fixture(scope="session")
def transport():
    """整个测试会话共用一个串口句柄。"""
    t = SerialTransport(port=DEFAULT_PORT, baudrate=BAUDRATE, timeout=0.5)
    yield t
    t.close()


@pytest.fixture()
def dut(transport):
    """每条用例一个 DUT 会话。"""
    d = ModbusDUT(transport)
    yield d
    d.close()
