#!/bin/bash
# STM32F407 Build Script

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

# Add STM32 toolchain to PATH
export PATH="/home/kevenchow/.local/share/stm32cube/bundles/gnu-tools-for-stm32/14.3.1+st.2/bin:$PATH"

echo "=============================="
echo "  Building project..."
echo "=============================="

# Create build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Configure and build
cmake ..
cmake --build .

echo ""
echo "=============================="
echo "  Build successful!"
echo "=============================="
echo ""
echo "Output files:"
ls -la *.elf *.hex *.bin 2>/dev/null || true
