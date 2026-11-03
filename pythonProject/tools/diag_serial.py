"""串口诊断脚本：单独跑，定位"框架为什么收不到响应"。

用法：
    python tools/diag_serial.py            # 默认 COM3
    python tools/diag_serial.py COM4
"""

import sys
import time

import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM3"
BAUD = 9600


def main() -> None:
    print(f"打开 {PORT} @ {BAUD} 8N1 ...")
    try:
        ser = serial.Serial(PORT, BAUD, bytesize=8, parity="N", stopbits=1, timeout=0.5)
    except Exception as exc:
        print(f"[失败] 打开串口出错: {exc}")
        return

    print(f"[成功] 串口已打开，is_open={ser.is_open}\n")

    # 1. 收一发：读 1 个寄存器
    frame = bytes.fromhex("010300000001 840A".replace(" ", ""))
    print(f"[发送] {frame.hex(' ').upper()}")
    ser.reset_input_buffer()
    ser.write(frame)
    ser.flush()

    time.sleep(0.05)
    resp = ser.read(64)
    if resp:
        print(f"[接收] {resp.hex(' ').upper()}  ({len(resp)} 字节)")
    else:
        print("[接收] 空 —— 板子没有回应")

    # 2. 再试一次（排除首次偶尔丢帧）
    time.sleep(0.1)
    print(f"\n[重试] {frame.hex(' ').upper()}")
    ser.write(frame)
    ser.flush()
    time.sleep(0.05)
    resp = ser.read(64)
    print(f"[接收] {resp.hex(' ').upper() if resp else '空'}")

    # 3. 打印串口状态，帮助判断是否有硬件流控/线路问题
    print(f"\n串口状态: in_waiting={ser.in_waiting}  is_open={ser.is_open}")
    ser.close()
    print("串口已关闭")


if __name__ == "__main__":
    main()
