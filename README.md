# 嵌入式协议自动化测试框架

> 一个分层的 **Modbus RTU 自动化测试框架**，把真实的 STM32 从机当作被测设备（DUT），
> 用 pytest 组织 111 条用例、YAML 数据驱动、主动注入异常，并把发现的缺陷沉淀成回归用例。
>
> 作者：张育宁（2027 届，通信工程）
> 定位：嵌入式测试 / 测试开发
> 技术栈：Python 3 + pytest + pyserial + PyYAML + pytest-html | C + Unity + gcov | Keil MDK | GitHub Actions

---

## 1. 项目概览

**一句话**：这是一个「分层架构 + 异常注入 + 缺陷闭环」的嵌入式协议测试框架，被测对象是一块真实的 STM32F103C8 板子（运行 Modbus RTU 从机固件）。

**项目由两部分组成**：

| 部分 | 位置 | 是什么 | 跑在哪 |
|---|---|---|---|
| **被测设备（DUT）** | `2-1stm32模板/` | STM32F103C8 上的 Modbus RTU 从机固件（0x03 读 / 0x06 写 / 异常码 / CRC） | 真实板子（Keil 编译，ST-Link 烧录） |
| **测试框架** | `pythonProject/` | 三层架构（transport / protocol / DUT）+ pytest 用例 | 电脑（PyCharm / CLI） |

**测试链路**：

```
  电脑（测试框架）                      板子（被测设备）
┌──────────────────────┐           ┌──────────────────────┐
│  pytest 用例          │           │  Modbus RTU 从机     │
│      ↓               │  串口请求   │      ↑               │
│  framework/dut.py    │ ────────► │  0x03 读寄存器        │
│      ↓               │  COM3     │  0x06 写寄存器        │
│  framework/modbus.py │  (9600    │  异常码 0x01/02/03    │
│  （组帧/解帧/CRC）     │   8N1)    │  CRC16 校验           │
│      ↓               │ ◄──────── │  空闲超时断帧(4ms)     │
│  framework/transport │  响应     │                      │
│  （串口收发）          │           │                      │
└──────────────────────┘           └──────────────────────┘
```

---

## 2. 架构设计（为什么要分层）

```
┌─────────────────────────────────────────────┐
│ tests/   pytest 用例（111 条）                │
│   只管"测什么"，不关心协议细节和硬件          │
├─────────────────────────────────────────────┤
│ framework/dut.py   DUT 层                    │
│   读寄存器 / 写寄存器 —— 业务动作            │
├─────────────────────────────────────────────┤
│ framework/modbus.py  协议层                  │
│   组帧 / 解帧 / CRC / 异常码 —— 纯字节操作   │
├─────────────────────────────────────────────┤
│ framework/transport.py  传输层               │
│   串口收发 —— 只负责字节进出                 │
└─────────────────────────────────────────────┘
```

**为什么这样分？**

| 场景 | 只改哪一层 | 例子 |
|---|---|---|
| 换被测设备（从 Modbus 换成别的协议） | `dut.py` | 换 DUT 不改用例 |
| 换通信方式（串口 → TCP / CAN） | `transport.py` | 换总线不改用例 |
| 加测试用例 | `tests/` 或 `data/cases.yaml` | 加用例不改代码 |
| 改协议细节（CRC、异常码） | `modbus.py` | 隔离协议变化 |

> 这就是「**换 DUT 不改用例**」的体现：111 条用例只依赖 `ModbusDUT` 的接口，不依赖具体是串口还是模拟器。

---

## 3. 目录结构

```
测试岗项目/
├─ README.md                    ← 你正在看
├─ 2-1stm32模板/                ← 被测设备（STM32 固件）
│  ├─ User/main.c               入口：初始化 + 轮询
│  ├─ BSP/                      板级支持包（我写的驱动）
│  │  ├─ bsp_usart.c/h          串口1：PA9/PA10，9600 8N1，环形缓冲
│  │  ├─ bsp_timer.c/h          TIM2：1ms 时基（断帧计时）
│  │  ├─ modbus_crc.c/h         CRC16（纯算法，PC 可测）
│  │  ├─ modbus_frame.c/h       帧解析 + 组帧（纯逻辑，PC 可测）★
│  │  ├─ modbus_slave.c/h       从机核心：只做串口搬运，逻辑在 frame 层
│  │  └─ bsp_crc.h              转发头（include 方便）
│  ├─ unit/                     PC 端 C 单元测试
│  │  ├─ unity/                 Unity 框架（ThrowTheSwitch）
│  │  ├─ test_crc16.c           CRC 测试 16 条
│  │  ├─ test_frame.c           帧解析测试 19 条 ★
│  │  ├─ Makefile               CI 用
│  │  └─ run.bat                一键：编译 + 跑测试 + 覆盖率
│  └─ 1.uvprojx                 Keil 工程
│
└─ pythonProject/               ← 测试框架
   ├─ framework/
   │  ├─ transport.py           传输层（串口 / 回环）
   │  ├─ modbus.py              协议层（组帧/解帧/CRC）
   │  ├─ dut.py                 DUT 层（ModbusDUT）
   │  └─ __init__.py
   ├─ tests/                    111 条 pytest 用例
   │  ├─ test_protocol_unit.py  21 条（离线，纯逻辑）
   │  ├─ test_functional.py     13 条（正向读写）
   │  ├─ test_boundary.py       19 条（边界：0/1/125/126）
   │  ├─ test_negative.py       27 条（异常注入）
   │  ├─ test_data_driven.py    14 条（YAML 数据驱动）
   │  ├─ test_robustness.py     6 条 （时序/粘包/超时）
   │  └─ test_regression.py     11 条（缺陷回归）
   ├─ data/cases.yaml           用例数据（加用例只改这个）
   ├─ tools/                    诊断脚本（diag / repro_bug001）
   ├─ defects.md                缺陷库
   ├─ pytest.ini
   └─ requirements.txt
```

---

## 4. 快速开始

### 4.1 环境要求

| 组件 | 版本/要求 | 用途 |
|---|---|---|
| Python | 3.10+（实测 3.13） | 跑 pytest |
| Keil MDK5 | V5.06+ | 编译固件 |
| ST-Link | — | 烧录 |
| USB-TTL（CH340/CP2102） | — | 连 COM3 |
| MinGW gcc | 8.1（实测） | PC 端 C 单元测试 |
| pytest / pyserial / PyYAML / pytest-html | 见 requirements.txt | 框架依赖 |

```bash
# 在 pythonProject 下
pip install -r requirements.txt
```

### 4.2 跑 C 单元测试（不碰硬件，随时可跑）

```bash
cd 2-1stm32模板/unit
# Windows：双击 run.bat
# 或命令行：
mingw32-make run
```

预期输出：
```
===== CRC =====
16 Tests 0 Failures 0 Ignored
===== Frame =====
19 Tests 0 Failures 0 Ignored
===== Coverage =====
File '../BSP/modbus_frame.c'
Lines executed:96.00% of 50
```

### 4.3 跑 Python 集成测试（需要板子）

```bash
cd pythonProject
.\.venv\Scripts\python.exe -m pytest -q
```

预期输出：
```
111 passed in 13.81s
```

> ⚠️ 跑之前：① 板子上电；② 串口助手关掉（它占 COM3）；③ 端口默认 COM3。
> 端口不是 COM3 时：`$env:MODBUS_PORT="COM4"; pytest -q`

### 4.4 分组跑

| 命令 | 说明 |
|---|---|
| `pytest -m smoke` | 冒烟（正向读写） |
| `pytest -m regression` | 回归（边界/异常/缺陷） |
| `pytest --collect-only -q` | 列出所有用例不执行 |
| `pytest --html=report.html --self-contained-html` | 生成 HTML 报告 |

---

## 5. 用例设计（111 条）

| 文件 | 条数 | 覆盖点 | 标记 |
|---|---|---|---|
| `test_protocol_unit.py` | 21 | CRC16 标准向量、组帧、解帧、异常码 | 离线 |
| `test_functional.py` | 13 | 0x03 读、0x06 写、写后回读、边界值 | smoke |
| `test_boundary.py` | 19 | 数量 0/1/125/126、地址 0/99/100、写边界 | regression |
| `test_negative.py` | 27 | CRC 篡改、非法功能码、非法地址/数量、错误从机地址 | regression |
| `test_data_driven.py` | 14 | **YAML 数据驱动**（加用例只改 cases.yaml） | regression |
| `test_robustness.py` | 6 | 帧间隔、粘包、超时、异常后恢复 | regression |
| `test_regression.py` | 11 | **缺陷回归**：每个 BUG 一条/一组 | regression |
| **合计** | **111** | | |

---

## 6. 缺陷库与缺陷闭环（核心价值）

在 `defects.md` 中登记了 **2 个实测缺陷**，每个都有：现象、复现步骤、根因、修复、回归用例。

### BUG-001：残缺帧导致从机永久失联（严重度：高）

- **现象**：从机收到一帧不完整报文（5 字节）后，**永久不再响应**，必须复位。
- **根因**：接收缓冲 `s_rx_len` 只在「攒够整帧」时清零；残缺帧残留的字节导致帧头永久错位，CRC 永远失败。
- **这是典型的「静默失效」**：设备看似活着（电源灯亮、串口可开），功能却死了。
- **修复**：改为「空闲超时断帧（3.5 字符 ≈ 4ms）+ 50ms 兜底强制清空」。

### BUG-002：按功能码推算帧长，无法处理粘包（严重度：中）

- **根因**：帧边界只能由「时间」或「长度+CRC」确定，靠字节数推断会连累 BUG-001。

### 缺陷闭环流程（实测证据）

```
① 建立基线：100 条全绿
② 故意引入缺陷（新手断帧写法）
③ 验证基线被打破：25 failed / 75 passed
④ 最小化复现：tools/repro_bug001.py，3 步复现
⑤ 定位根因：s_rx_len 只在攒够整帧时清零
⑥ 修复：空闲超时断帧 + 兜底清空
⑦ 验证修复：同一脚本从「复现成功」变「未复现」
⑧ 补回归用例：11 条锁死
⑨ 全量回归：111 条全绿
```

---

## 7. 实测数据（全部真实测出，非估算）

### 7.1 C 单元测试覆盖率（Unity + gcov）

| 被测模块 | 行覆盖率 | 分支覆盖率 | 说明 |
|---|---|---|---|
| `modbus_frame.c`（帧解析+组帧） | **96%**（50 行） | 100% | 协议核心 |
| `modbus_crc.c`（CRC16） | **90.9%**（11 行） | 100% | 校验算法 |

> 未覆盖的分支是**故意不测的防御性代码**（空指针、错误帧长），实际通信中几乎不可达。

### 7.2 手动 vs 自动（秒表实测）

| 项目 | 条数 | 耗时 | 平均每条 |
|---|---|---|---|
| 手动（串口助手） | 8 条 | **44.1 秒** | 5.5 秒 |
| 自动（pytest） | 111 条 | **14.5 秒** | 0.13 秒 |

**效率提升 ≈ 42 倍**（按同一条数折算：手动 111 条约需 10.2 分钟）。

> 更重要的是：手动只能测正向；边界、异常、粘包、缺陷回归这些**手动根本做不了**（人眼来不及比对 CRC）。

---

## 8. 截图

> 以下截图请自行补充（用你实际跑出来的）：
>
> - [ ] `unit/run.bat` 输出（35 条 PASS + 覆盖率）
> - [ ] `pytest -q` 输出（111 passed）
> - [ ] 串口助手手动测试（8 条报文往返）
> - [ ] 架构图（可用 Mermaid 生成后导出）

---

## 9. 面试要点（8 问速答）

| 问题 | 答案 |
|---|---|
| 为什么分三层？ | 换 DUT 只改 dut 层，换总线只改 transport 层 |
| fixture 解决什么？ | 串口独占、失败自动复位、用例隔离 |
| 数据驱动好处？ | 加用例不改代码，产品/测试能直接看懂 YAML |
| 你注入哪些异常？ | CRC 篡改、非法功能码、非法地址/数量、粘包、残缺帧、超时 |
| 最有价值的缺陷？ | BUG-001 静默失效（现象/复现/根因/修复/回归） |
| 覆盖率为什么不是 100%？ | 剩下 4% 是防御性代码，实际不可达 |
| CI 为什么不跑集成测试？ | CI 没有板子，这正是测试分层的理由 |
| 模拟器和真机结论一样吗？ | 不一样：模拟器验框架逻辑，真机验时序与硬件缺陷 |

---

## 10. 后续规划

- [ ] CI（GitHub Actions）：跑 PC 端 C 单元测试 + 离线 Python 单测
- [ ] 纯 Python 从机模拟器（`simulator/modbus_slave_sim.py`）：无硬件也能跑
- [ ] 推送 GitHub 开源

---

## 11. 致谢与免责

- Unity 框架来自 [ThrowTheSwitch/Unity](https://github.com/ThrowTheSwitch/Unity)，遵循 MIT 许可（LICENSE 已保留在 `unit/unity/LICENSE.txt`）。
- 本项目所有测试数据、覆盖率、耗时、缺陷均为**实测结果**，无任何编造。
