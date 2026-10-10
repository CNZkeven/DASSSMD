# UART GUI Port and ADC Simulator Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the Python UART GUI from the sibling `uart` experiment into this integrated project and improve the ADC waveform with a smooth simulator.

**Architecture:** Keep the GUI as a host-side Python package under `uart_gui/`, independent from the STM32 firmware. Extend parsing so the GUI understands the integrated firmware's `ADC: 0xNNNN, Voltage: X.XXXV` text, and add a deterministic ADC simulator that can feed the existing plot widget when hardware is disconnected.

**Tech Stack:** Python 3, PySide6, pyserial, pyqtgraph, numpy, pytest, STM32F407 firmware build via CMake.

---

### Task 1: Copy GUI Package

**Files:**
- Create: `uart_gui/`
- Create: `run_gui.sh`

- [ ] Copy source GUI files from `/home/kevenchow/桌面/f407/四个项实验代码/uart/uart_gui`.
- [ ] Exclude `__pycache__` and log output.
- [ ] Add a project-local GUI launcher.

### Task 2: Parser Support for Integrated Firmware ADC Lines

**Files:**
- Modify: `uart_gui/parser.py`
- Test: `uart_gui/tests/test_parser.py`

- [ ] Add failing tests for raw decimal ADC, hex ADC, and integrated firmware ADC log lines.
- [ ] Extend `DataParser.parse_adc()` to extract the ADC raw value from these formats.

### Task 3: Smooth ADC Simulator

**Files:**
- Create: `uart_gui/adc_simulator.py`
- Test: `uart_gui/tests/test_adc_simulator.py`

- [ ] Add tests proving generated values stay in `0..4095`, are deterministic with a seed, and change smoothly.
- [ ] Implement a sine + slow drift + bounded noise simulator.

### Task 4: GUI Wiring

**Files:**
- Modify: `uart_gui/app.py`
- Modify: `uart_gui/plot_widget.py`

- [ ] Add simulator controls in the ADC panel.
- [ ] Feed simulated values through a timer into the existing plot widget.
- [ ] Keep real serial ADC data working.

### Task 5: Verification

**Files:**
- Verify: `uart_gui/tests/`
- Verify: `tests/test_static_contracts.py`
- Verify: `build/`

- [ ] Run `python -m pytest uart_gui/tests -q`.
- [ ] Run `pytest -q tests/test_static_contracts.py`.
- [ ] Run `PATH="/home/kevenchow/.local/share/stm32cube/bundles/gnu-tools-for-stm32/14.3.1+st.2/bin:$PATH" cmake --build build`.
