"""数据驱动用例：用例数据来自 data/cases.yaml。

加一条用例只需改 YAML，不需要改这里一行代码。
"""

from pathlib import Path

import pytest
import yaml

from framework import modbus

pytestmark = [pytest.mark.regression]

CASES_FILE = Path(__file__).resolve().parent.parent / "data" / "cases.yaml"


def _load(name: str) -> list[dict]:
    with CASES_FILE.open(encoding="utf-8") as f:
        return yaml.safe_load(f)[name]


READ_CASES = _load("read_cases")
WRITE_CASES = _load("write_cases")


def _case_id(case: dict) -> str:
    return case["name"]


@pytest.mark.parametrize("case", READ_CASES, ids=_case_id)
def test_read_from_yaml(dut, case):
    """按 YAML 里的数据读寄存器，验证正常值或异常码。"""
    start = case["start"]
    qty = case["qty"]
    expect = case["expect"]

    frame = modbus.build_read_holding(dut.slave_addr, start, qty)

    if "exc" in expect:
        raw = dut.read_raw(frame, expect_len=5)
        parsed = modbus.parse_response(raw)
        assert parsed.get("exception") == expect["exc"]
    else:
        raw = dut.read_raw(frame, expect_len=5 + qty * 2)
        parsed = modbus.parse_response(raw)
        assert "exception" not in parsed, f"期望正常响应，实际异常 {parsed}"

        expected_values = expect.get("ok")
        if expected_values is not None:
            assert parsed["data"] == expected_values


@pytest.mark.parametrize("case", WRITE_CASES, ids=_case_id)
def test_write_from_yaml(dut, case):
    """按 YAML 里的数据写寄存器，并回读确认。"""
    reg = case["reg"]
    value = case["value"]

    dut.write_reg(reg, value)
    assert dut.read_regs(reg, 1) == [value], f"寄存器 {reg} 回读不一致"
