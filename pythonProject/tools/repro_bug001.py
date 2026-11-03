"""BUG-001 最小复现脚本：逐步验证"残缺帧导致永久死锁"。

用法：
    python tools/repro_bug001.py COM3
"""

import sys
import time

import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM3"
BAUD = 9600

NORMAL = bytes.fromhex("010300000001840A")
TRUNCATED = bytes.fromhex("0103000000")      # 只有 5 字节，缺数量和 CRC


def ask(ser, frame, label, timeout=0.5):
    ser.reset_input_buffer()
    ser.write(frame)
    ser.flush()
    time.sleep(0.05)
    resp = ser.read(64)
    status = "有响应" if resp else "无响应"
    print(f"  {label:<28} 发送 {frame.hex(' ').upper():<24} -> {status}  {resp.hex(' ').upper()}")
    return len(resp) > 0


def main():
    ser = serial.Serial(PORT, BAUD, timeout=0.5)
    print(f"=== BUG-001 复现 === 端口 {PORT}\n")

    ok1 = ask(ser, NORMAL, "① 正常读（基线）")

    print("\n  >>> 现在发送一帧残缺报文（只有 5 字节）")
    ser.write(TRUNCATED)
    ser.flush()
    time.sleep(0.3)

    ok2 = ask(ser, NORMAL, "② 残缺帧后正常读")

    print("\n  >>> 再试一次，确认不是偶发")
    ok3 = ask(ser, NORMAL, "③ 再发一次正常读")

    print("\n=== 结论 ===")
    if ok1 and not ok2 and not ok3:
        print("  ✅ 复现成功：BUG-001 存在")
        print("     现象：发送残缺帧后，从机永久失去响应")
        print("     恢复：需要按复位键")
    elif ok1 and ok2:
        print("  ❌ 未复现：残缺帧后仍能正常响应（固件可能已修复）")
    else:
        print("  ⚠️  基线就失败，请先检查板子是否上电、串口是否正确")

    ser.close()


if __name__ == "__main__":
    main()
