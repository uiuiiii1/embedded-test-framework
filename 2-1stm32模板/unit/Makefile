# C 单元测试构建 —— Linux / Windows 通用
#
# Windows（MinGW）:
#   mingw32-make run
# Linux / CI:
#   make run
#
# run 目标：编译两个测试 -> 运行 -> 生成覆盖率文本 -> 尝试生成 HTML

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -O0 \
          -fprofile-arcs -ftest-coverage \
          -I. -Iunity -I../BSP
LDFLAGS = -lgcov --coverage

BSP_DIR = ../BSP

# 两个测试程序（不带 .exe，gcc 在 Windows 下自动加扩展名）
T1 = test_crc16
T2 = test_frame

.PHONY: all run cov clean

all: $(T1) $(T2)

$(T1): test_crc16.c unity/unity.c $(BSP_DIR)/modbus_crc.c
	$(CC) $(CFLAGS) $^ -o $(T1) $(LDFLAGS)

$(T2): test_frame.c unity/unity.c $(BSP_DIR)/modbus_frame.c $(BSP_DIR)/modbus_crc.c
	$(CC) $(CFLAGS) $^ -o $(T2) $(LDFLAGS)

run: $(T1) $(T2)
	@echo "===== 测试 1：CRC16 ====="
	./$(T1)
	@echo ""
	@echo "===== 测试 2：帧解析 ====="
	./$(T2)
	@echo ""
	@echo "===== 覆盖率（gcov 文本） ====="
	gcov -b -c -o . $(BSP_DIR)/modbus_crc.c
	gcov -b -c -o . $(BSP_DIR)/modbus_frame.c

# 生成 HTML 覆盖率报告（需要 lcov；CI/Ubuntu 装了才有）
cov: run
	@if command -v lcov >/dev/null 2>&1; then \
		lcov --capture --directory . --directory ../BSP --output-file coverage.info --quiet; \
		lcov --remove coverage.info '*/unity/*' '*/test_*' --output-file coverage.info --quiet; \
		lcov --extract coverage.info '*modbus_frame.c' '*modbus_crc.c' --output-file coverage.info --quiet; \
		genhtml coverage.info --output-directory coverage_report --quiet; \
		echo "HTML 报告已生成：coverage_report/index.html"; \
	else \
		echo "[跳过] 未安装 lcov，仅生成了 .gcov 文本报告"; \
	fi

clean:
	rm -f $(T1) $(T2) *.o *.gcda *.gcno *.gcov coverage.info
	rm -rf coverage_report
	@echo "清理完成"
