#!/bin/bash
#
# ZHSX_RTOS 烧录（OpenOCD + 板载 CMSIS-DAP）
#
# ★ 必须用"复位下连接"，原因见 openocd.cfg 里的说明：
#   PA14 被用作 Y 轴 DIR，固件一跑起来 SWCLK 就没了。
#
# 判断成功与否只看输出里的 "verified N bytes"，不要看退出码：
#   1) 以 shutdown 结尾时 OpenOCD 恒返回 0；
#   2) reset run 之后必然报 "Error connecting DP: cannot read IDR"，
#      因为固件已经在跑了，这是预期现象。

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
ELF_FILE="${BUILD_DIR}/zhsx.elf"
OPENOCD_CFG="${SCRIPT_DIR}/openocd.cfg"

if [ ! -f "${ELF_FILE}" ]; then
    echo "错误：找不到 ${ELF_FILE}"
    echo "请先编译：bash build.sh"
    exit 1
fi

echo "=============================="
echo "  烧录：${ELF_FILE}"
echo "=============================="

LOG=$(mktemp)
trap 'rm -f "$LOG"' EXIT

openocd -f "${OPENOCD_CFG}" \
    -c "init" \
    -c "reset halt" \
    -c "flash write_image erase ${ELF_FILE}" \
    -c "verify_image ${ELF_FILE}" \
    -c "reset run" \
    -c "shutdown" 2>&1 | tee "$LOG"

echo ""

if grep -qE "verified [0-9]+ bytes" "$LOG"; then
    echo "=============================="
    echo "  烧录成功"
    echo "  $(grep -oE 'verified [0-9]+ bytes[^(]*' "$LOG" | tail -1 | sed 's/[[:space:]]*$//')"
    echo "=============================="
    exit 0
fi

echo "=============================="
echo "  烧录失败"
echo "  请确认开发板已连接、未被其它程序占用。"
echo "=============================="
exit 1
