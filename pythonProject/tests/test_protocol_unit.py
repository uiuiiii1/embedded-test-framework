"""协议层单元测试：不碰硬件，纯逻辑验证。CI 里跑的就是这些。"""

import pytest

from framework import modbus


class TestCRC16:
    @pytest.mark.parametrize(
        "hexstr, expected",
        [
            ("01030000000A", 0xCDC5),  # Modbus 标准测试向量
            ("00", 0x40BF),
            ("", 0xFFFF),
            ("010300000004", 0x0944),
            ("010600051234", 0xBC94),
        ],
    )
    def test_known_vectors(self, hexstr, expected):
        assert modbus.crc16_modbus(bytes.fromhex(hexstr)) == expected

    def test_append_crc_little_endian(self):
        frame = modbus.append_crc(bytes.fromhex("01030000000A"))
        # CRC=0xC5CD，追加时低字节 C5 在前
        assert frame[-2:] == bytes.fromhex("C5CD")

    def test_check_crc_ok(self):
        assert modbus.check_crc(bytes.fromhex("01030000000AC5CD"))

    def test_check_crc_bad(self):
        assert not modbus.check_crc(bytes.fromhex("01030000000AFFFF"))

    def test_check_crc_too_short(self):
        assert not modbus.check_crc(b"\x01")

    def test_crc_is_deterministic(self):
        """同一帧反复计算结果必须稳定（防止实现里有隐藏状态）。"""
        data = bytes.fromhex("010300000064")
        assert modbus.crc16_modbus(data) == modbus.crc16_modbus(data)


class TestBuildFrame:
    def test_build_read_holding(self):
        assert modbus.build_read_holding(1, 0, 10).hex(" ").upper() == "01 03 00 00 00 0A C5 CD"

    def test_build_read_holding_offset(self):
        frame = modbus.build_read_holding(1, 0x1234, 0x0002)
        assert frame[:6].hex(" ").upper() == "01 03 12 34 00 02"
        assert modbus.check_crc(frame)

    def test_build_write_single(self):
        assert modbus.build_write_single(1, 5, 0x1234).hex(" ").upper() == "01 06 00 05 12 34 94 BC"


class TestParseResponse:
    def test_parse_read_ok(self):
        parsed = modbus.parse_response(bytes.fromhex("010308000000010002000349D6"))
        assert parsed == {"addr": 1, "func": 0x03, "data": [0, 1, 2, 3]}

    def test_parse_write_ok(self):
        parsed = modbus.parse_response(bytes.fromhex("01060005123494BC"))
        assert parsed == {"addr": 1, "func": 0x06, "value": 0x1234}

    @pytest.mark.parametrize(
        "hexstr, func, code",
        [
            ("0187018230", 0x07, 0x01),
            ("018302C0F1", 0x03, 0x02),
            ("0183030131", 0x03, 0x03),
        ],
    )
    def test_parse_exception(self, hexstr, func, code):
        parsed = modbus.parse_response(bytes.fromhex(hexstr))
        assert parsed["func"] == func
        assert parsed["exception"] == code

    def test_parse_bad_crc_raises(self):
        with pytest.raises(ValueError, match="CRC"):
            modbus.parse_response(bytes.fromhex("010308000000010002000349FF"))

    def test_parse_too_short_raises(self):
        with pytest.raises(ValueError, match="太短"):
            modbus.parse_response(bytes.fromhex("0103"))

    def test_parse_unsupported_func_raises(self):
        frame = modbus.append_crc(bytes.fromhex("0104010203"))
        with pytest.raises(ValueError, match="不支持的功能码"):
            modbus.parse_response(frame)
