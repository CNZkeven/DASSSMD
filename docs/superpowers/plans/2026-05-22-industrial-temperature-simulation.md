# Industrial Temperature Simulation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the sixth "Industrial Temp" task with on-board PID temperature simulation, touch sliders, chart rendering, and red/green LED status.

**Architecture:** Add a pure control module in `User/task/industrial_temp.[ch]` for PID state, fixed-point thermal simulation, parameter ranges, touch-to-slider mapping, and chart history. Extend the existing `UI_Task` / `Worker_Task` architecture so the UI routes touches and draws the task, while the worker updates the simulation and LED every 200 ms.

**Tech Stack:** STM32F407 StdPeriph, uCOS-III, NT35510 LCD, GT9xx touch, C99, CMake, pytest static tests, host `gcc` + `ctypes` model tests.

---

## File Structure

- Modify `User/app_cfg.h`: add `TASK_INDUSTRIAL_TEMP` and `TASK_BUTTON_COUNT`.
- Create `User/task/industrial_temp.h`: public constants, state struct, parameter enum, and API.
- Create `User/task/industrial_temp.c`: fixed-point PID model, thermal update, clamping, history, touch slider mapping.
- Modify `User/ui/ui_draw.h`: industrial temperature colors, layout constants, and draw function prototypes.
- Modify `User/ui/ui_draw.c`: six-button menu, industrial temperature chart/sliders/value drawing.
- Modify `User/ui/ui_task.c`: six-button hit testing, industrial-temperature page selection, slider touch routing.
- Modify `User/task/worker_task.c`: initialize/reset/update industrial simulation and set red/green LED.
- Modify `CMakeLists.txt`: build `User/task/industrial_temp.c`.
- Modify `tests/test_static_contracts.py`: static contracts for menu, touch routing, worker integration, and no UART dependency.
- Create `tests/test_industrial_temp_model.py`: host-compiled model behavior tests for default values, clamping, heating/cooling, and in-range detection.

---

### Task 1: Add Failing Static Contracts For The Sixth Task

**Files:**
- Modify: `tests/test_static_contracts.py`

- [ ] **Step 1: Write the failing static tests**

Append these tests to `tests/test_static_contracts.py`, and update existing tests that currently hard-code five buttons:

```python
def test_industrial_temp_task_id_and_button_count_are_declared():
    app_cfg = read("User/app_cfg.h")

    enum_match = re.search(r"typedef enum\s*\{(?P<body>.*?)\}\s*TaskID_t;", app_cfg, re.S)
    assert enum_match, "TaskID_t enum not found"
    enum_names = re.findall(r"\bTASK_[A-Z_]+\b", enum_match.group("body"))

    assert "TASK_INDUSTRIAL_TEMP" in enum_names
    assert enum_names.index("TASK_INDUSTRIAL_TEMP") < enum_names.index("TASK_MAX")
    assert "#define TASK_BUTTON_COUNT      6" in app_cfg


def test_industrial_temp_button_is_on_main_menu():
    source = read("User/ui/ui_draw.c")

    buttons_match = re.search(
        r"const Button_t task_buttons\[TASK_BUTTON_COUNT\]\s*=\s*\{(?P<body>.*?)\};",
        source,
        re.S,
    )
    assert buttons_match, "task_buttons table not found"
    body = buttons_match.group("body")

    assert 'TASK_INDUSTRIAL_TEMP, "Industrial Temp", "PID Temperature Sim"' in body

    entries = re.findall(
        r"\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*TASK_[A-Z_]+,\s*\"([^\"]+)\",\s*\"([^\"]+)\"\s*\}",
        body,
    )
    assert len(entries) == 6
    for entry in entries:
        x, y, width, height = map(int, entry[:4])
        assert x + width <= 480
        assert y + height <= 800
```

Update `test_ui_ignores_main_menu_task_buttons_while_a_task_is_running()` so the final assertion checks the new loop:

```python
assert body.index("else {") < body.index("for (int i = 0; i < TASK_BUTTON_COUNT; i++)")
```

Update `test_ui_text_and_buttons_are_bounded_to_portrait_screen()` so the regex accepts `TASK_BUTTON_COUNT`:

```python
buttons_match = re.search(
    r"const Button_t task_buttons\[TASK_BUTTON_COUNT\]\s*=\s*\{(?P<body>.*?)\};",
    source,
    re.S,
)
```

- [ ] **Step 2: Run tests to verify they fail**

Run:

```bash
pytest -q tests/test_static_contracts.py
```

Expected: FAIL because `TASK_INDUSTRIAL_TEMP`, `TASK_BUTTON_COUNT`, and the six-button menu do not exist yet.

- [ ] **Step 3: Commit the failing tests**

```bash
git add tests/test_static_contracts.py
git commit -m "test: add industrial temperature menu contracts"
```

---

### Task 2: Add The Sixth Task ID And Menu Button

**Files:**
- Modify: `User/app_cfg.h`
- Modify: `User/ui/ui_draw.c`
- Modify: `User/ui/ui_task.c`

- [ ] **Step 1: Implement task ID and button count**

In `User/app_cfg.h`, add the button count and task ID:

```c
#define TASK_BUTTON_COUNT      6

typedef enum {
    TASK_NONE = 0,
    TASK_BUZZER,
    TASK_UART,
    TASK_ADC,
    TASK_GPIO_LED,
    TASK_ADC_LED,
    TASK_INDUSTRIAL_TEMP,
    TASK_MAX
} TaskID_t;
```

- [ ] **Step 2: Replace the five-button menu table**

In `User/ui/ui_draw.c`, replace `task_buttons[5]` with:

```c
const Button_t task_buttons[TASK_BUTTON_COUNT] = {
    {40,  78,  400, 80, TASK_BUZZER,           "Buzzer",          "Buzzer ON/OFF Test"},
    {40,  190, 400, 80, TASK_UART,             "UART",            "Serial Communication"},
    {40,  302, 400, 80, TASK_ADC,              "ADC",             "Voltage Measurement"},
    {40,  414, 400, 80, TASK_GPIO_LED,         "GPIO LED",        "LED Blink Test"},
    {40,  526, 400, 80, TASK_ADC_LED,          "ADC+LED",         "Voltage LED Control"},
    {40,  638, 400, 80, TASK_INDUSTRIAL_TEMP,  "Industrial Temp", "PID Temperature Sim"},
};
```

Update `UI_DrawMainMenu()` so it iterates over `TASK_BUTTON_COUNT`:

```c
for (i = 0; i < TASK_BUTTON_COUNT; i++) {
    UI_DrawButton(&task_buttons[i], 0);
}
```

- [ ] **Step 3: Update UI hit testing**

In `User/ui/ui_task.c`, replace the extern and loop count:

```c
extern const Button_t task_buttons[TASK_BUTTON_COUNT];
```

```c
for (int i = 0; i < TASK_BUTTON_COUNT; i++) {
    if (x >= task_buttons[i].x &&
        x <= task_buttons[i].x + task_buttons[i].width &&
        y >= task_buttons[i].y &&
        y <= task_buttons[i].y + task_buttons[i].height) {

        g_current_task = task_buttons[i].task_id;
        UI_DrawTaskView(&task_buttons[i]);
        return;
    }
}
```

- [ ] **Step 4: Run tests to verify menu contracts pass**

Run:

```bash
pytest -q tests/test_static_contracts.py::test_industrial_temp_task_id_and_button_count_are_declared tests/test_static_contracts.py::test_industrial_temp_button_is_on_main_menu tests/test_static_contracts.py::test_ui_ignores_main_menu_task_buttons_while_a_task_is_running tests/test_static_contracts.py::test_ui_text_and_buttons_are_bounded_to_portrait_screen
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add User/app_cfg.h User/ui/ui_draw.c User/ui/ui_task.c
git commit -m "feat: add industrial temperature menu entry"
```

---

### Task 3: Add Failing Model Behavior Tests

**Files:**
- Create: `tests/test_industrial_temp_model.py`

- [ ] **Step 1: Write host-compiled model tests**

Create `tests/test_industrial_temp_model.py`:

```python
#!/usr/bin/env python3
from ctypes import CDLL, POINTER, Structure, c_int16, c_uint8, c_uint16
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[1]

PARAM_SET_TEMP = 0
PARAM_KP = 1
PARAM_KI = 2
PARAM_KD = 3


class IndustrialTempState(Structure):
    _fields_ = [
        ("set_temp_dC", c_int16),
        ("actual_temp_dC", c_int16),
        ("error_dC", c_int16),
        ("kp_x10", c_int16),
        ("ki_x100", c_int16),
        ("kd_x10", c_int16),
        ("heater_percent", c_uint8),
        ("history_count", c_uint16),
        ("history_index", c_uint16),
    ]


def build_model(tmp_path):
    lib_path = tmp_path / "libindustrial_temp.so"
    subprocess.run(
        [
            "gcc",
            "-std=c99",
            "-Wall",
            "-Wextra",
            "-shared",
            "-fPIC",
            "-I",
            str(ROOT / "User/task"),
            str(ROOT / "User/task/industrial_temp.c"),
            "-o",
            str(lib_path),
        ],
        check=True,
    )
    lib = CDLL(str(lib_path))
    lib.IndustrialTemp_GetState.restype = POINTER(IndustrialTempState)
    lib.IndustrialTemp_SetParam.argtypes = [c_uint8, c_int16]
    lib.IndustrialTemp_SetParamFromTouch.argtypes = [c_uint16, c_uint16]
    lib.IndustrialTemp_SetParamFromTouch.restype = c_uint8
    lib.IndustrialTemp_IsInRange.restype = c_uint8
    return lib


def state(lib):
    return lib.IndustrialTemp_GetState().contents


def test_default_state_and_parameter_clamping(tmp_path):
    lib = build_model(tmp_path)

    lib.IndustrialTemp_Init()
    current = state(lib)

    assert current.set_temp_dC == 800
    assert current.actual_temp_dC == 250
    assert current.kp_x10 == 20
    assert current.ki_x100 == 10
    assert current.kd_x10 == 5
    assert current.heater_percent == 0

    lib.IndustrialTemp_SetParam(PARAM_SET_TEMP, 1300)
    lib.IndustrialTemp_SetParam(PARAM_KP, 150)
    lib.IndustrialTemp_SetParam(PARAM_KI, -10)
    lib.IndustrialTemp_SetParam(PARAM_KD, 80)
    current = state(lib)

    assert current.set_temp_dC == 1200
    assert current.kp_x10 == 100
    assert current.ki_x100 == 0
    assert current.kd_x10 == 50


def test_pid_update_heats_then_reports_in_range(tmp_path):
    lib = build_model(tmp_path)

    lib.IndustrialTemp_Init()
    lib.IndustrialTemp_SetParam(PARAM_SET_TEMP, 600)
    lib.IndustrialTemp_SetParam(PARAM_KP, 60)
    lib.IndustrialTemp_SetParam(PARAM_KI, 25)
    lib.IndustrialTemp_SetParam(PARAM_KD, 5)

    for _ in range(120):
        lib.IndustrialTemp_Update()

    current = state(lib)
    assert current.actual_temp_dC > 500
    assert abs(current.error_dC) <= 80
    assert current.history_count > 0

    for _ in range(120):
        lib.IndustrialTemp_Update()

    assert lib.IndustrialTemp_IsInRange() == 1


def test_touch_slider_mapping_updates_expected_parameters(tmp_path):
    lib = build_model(tmp_path)

    lib.IndustrialTemp_Init()

    assert lib.IndustrialTemp_SetParamFromTouch(420, 440) == 1
    assert state(lib).set_temp_dC == 1200

    assert lib.IndustrialTemp_SetParamFromTouch(120, 505) == 1
    assert state(lib).kp_x10 == 0

    assert lib.IndustrialTemp_SetParamFromTouch(270, 570) == 1
    assert 45 <= state(lib).ki_x100 <= 55

    assert lib.IndustrialTemp_SetParamFromTouch(999, 635) == 1
    assert state(lib).kd_x10 == 50

    assert lib.IndustrialTemp_SetParamFromTouch(40, 760) == 0
```

- [ ] **Step 2: Run tests to verify they fail**

Run:

```bash
pytest -q tests/test_industrial_temp_model.py
```

Expected: FAIL because `User/task/industrial_temp.c` does not exist.

- [ ] **Step 3: Commit the failing tests**

```bash
git add tests/test_industrial_temp_model.py
git commit -m "test: add industrial temperature model tests"
```

---

### Task 4: Implement The Industrial Temperature Model

**Files:**
- Create: `User/task/industrial_temp.h`
- Create: `User/task/industrial_temp.c`

- [ ] **Step 1: Create the public header**

Create `User/task/industrial_temp.h`:

```c
#ifndef __INDUSTRIAL_TEMP_H
#define __INDUSTRIAL_TEMP_H

#include <stdint.h>

#define INDUSTRIAL_TEMP_HISTORY_LEN       120u

#define INDUSTRIAL_TEMP_AMBIENT_DC        250
#define INDUSTRIAL_TEMP_SET_DEFAULT_DC    800
#define INDUSTRIAL_TEMP_SET_MIN_DC        200
#define INDUSTRIAL_TEMP_SET_MAX_DC        1200

#define INDUSTRIAL_TEMP_KP_DEFAULT_X10    20
#define INDUSTRIAL_TEMP_KP_MIN_X10        0
#define INDUSTRIAL_TEMP_KP_MAX_X10        100

#define INDUSTRIAL_TEMP_KI_DEFAULT_X100   10
#define INDUSTRIAL_TEMP_KI_MIN_X100       0
#define INDUSTRIAL_TEMP_KI_MAX_X100       100

#define INDUSTRIAL_TEMP_KD_DEFAULT_X10    5
#define INDUSTRIAL_TEMP_KD_MIN_X10        0
#define INDUSTRIAL_TEMP_KD_MAX_X10        50

#define INDUSTRIAL_TEMP_IN_RANGE_DC       50
#define INDUSTRIAL_TEMP_PLOT_MIN_DC       0
#define INDUSTRIAL_TEMP_PLOT_MAX_DC       1300

#define INDUSTRIAL_TEMP_SLIDER_X          120u
#define INDUSTRIAL_TEMP_SLIDER_W          300u
#define INDUSTRIAL_TEMP_SLIDER_SET_Y      440u
#define INDUSTRIAL_TEMP_SLIDER_KP_Y       505u
#define INDUSTRIAL_TEMP_SLIDER_KI_Y       570u
#define INDUSTRIAL_TEMP_SLIDER_KD_Y       635u
#define INDUSTRIAL_TEMP_SLIDER_H          34u

typedef enum {
    INDUSTRIAL_TEMP_PARAM_SET_TEMP = 0,
    INDUSTRIAL_TEMP_PARAM_KP,
    INDUSTRIAL_TEMP_PARAM_KI,
    INDUSTRIAL_TEMP_PARAM_KD
} IndustrialTempParam_t;

typedef struct {
    int16_t set_temp_dC;
    int16_t actual_temp_dC;
    int16_t error_dC;
    int16_t kp_x10;
    int16_t ki_x100;
    int16_t kd_x10;
    uint8_t heater_percent;
    uint16_t history_count;
    uint16_t history_index;
    int16_t set_history_dC[INDUSTRIAL_TEMP_HISTORY_LEN];
    int16_t actual_history_dC[INDUSTRIAL_TEMP_HISTORY_LEN];
} IndustrialTempState_t;

void IndustrialTemp_Init(void);
void IndustrialTemp_Reset(void);
void IndustrialTemp_Update(void);
void IndustrialTemp_SetParam(IndustrialTempParam_t param, int16_t value);
uint8_t IndustrialTemp_SetParamFromTouch(uint16_t x, uint16_t y);
const IndustrialTempState_t *IndustrialTemp_GetState(void);
uint8_t IndustrialTemp_IsInRange(void);

#endif /* __INDUSTRIAL_TEMP_H */
```

- [ ] **Step 2: Create the model implementation**

Create `User/task/industrial_temp.c` with these functions and constants:

```c
#include "industrial_temp.h"

static IndustrialTempState_t g_temp_state;
static int32_t g_integral_dC;
static int16_t g_previous_error_dC;

static int16_t IndustrialTemp_ClampI16(int32_t value, int16_t min_value, int16_t max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return (int16_t)value;
}

static uint8_t IndustrialTemp_ClampU8(int32_t value, uint8_t min_value, uint8_t max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return (uint8_t)value;
}

static void IndustrialTemp_PushHistory(void) {
    g_temp_state.set_history_dC[g_temp_state.history_index] = g_temp_state.set_temp_dC;
    g_temp_state.actual_history_dC[g_temp_state.history_index] = g_temp_state.actual_temp_dC;
    g_temp_state.history_index++;
    if (g_temp_state.history_index >= INDUSTRIAL_TEMP_HISTORY_LEN) {
        g_temp_state.history_index = 0u;
    }
    if (g_temp_state.history_count < INDUSTRIAL_TEMP_HISTORY_LEN) {
        g_temp_state.history_count++;
    }
}

void IndustrialTemp_Reset(void) {
    uint16_t i;

    g_temp_state.set_temp_dC = INDUSTRIAL_TEMP_SET_DEFAULT_DC;
    g_temp_state.actual_temp_dC = INDUSTRIAL_TEMP_AMBIENT_DC;
    g_temp_state.error_dC = 0;
    g_temp_state.kp_x10 = INDUSTRIAL_TEMP_KP_DEFAULT_X10;
    g_temp_state.ki_x100 = INDUSTRIAL_TEMP_KI_DEFAULT_X100;
    g_temp_state.kd_x10 = INDUSTRIAL_TEMP_KD_DEFAULT_X10;
    g_temp_state.heater_percent = 0u;
    g_temp_state.history_count = 0u;
    g_temp_state.history_index = 0u;
    g_integral_dC = 0;
    g_previous_error_dC = 0;

    for (i = 0u; i < INDUSTRIAL_TEMP_HISTORY_LEN; i++) {
        g_temp_state.set_history_dC[i] = g_temp_state.set_temp_dC;
        g_temp_state.actual_history_dC[i] = g_temp_state.actual_temp_dC;
    }
    IndustrialTemp_PushHistory();
}

void IndustrialTemp_Init(void) {
    IndustrialTemp_Reset();
}

void IndustrialTemp_SetParam(IndustrialTempParam_t param, int16_t value) {
    switch (param) {
        case INDUSTRIAL_TEMP_PARAM_SET_TEMP:
            g_temp_state.set_temp_dC = IndustrialTemp_ClampI16(value,
                                                              INDUSTRIAL_TEMP_SET_MIN_DC,
                                                              INDUSTRIAL_TEMP_SET_MAX_DC);
            break;
        case INDUSTRIAL_TEMP_PARAM_KP:
            g_temp_state.kp_x10 = IndustrialTemp_ClampI16(value,
                                                          INDUSTRIAL_TEMP_KP_MIN_X10,
                                                          INDUSTRIAL_TEMP_KP_MAX_X10);
            break;
        case INDUSTRIAL_TEMP_PARAM_KI:
            g_temp_state.ki_x100 = IndustrialTemp_ClampI16(value,
                                                           INDUSTRIAL_TEMP_KI_MIN_X100,
                                                           INDUSTRIAL_TEMP_KI_MAX_X100);
            break;
        case INDUSTRIAL_TEMP_PARAM_KD:
            g_temp_state.kd_x10 = IndustrialTemp_ClampI16(value,
                                                          INDUSTRIAL_TEMP_KD_MIN_X10,
                                                          INDUSTRIAL_TEMP_KD_MAX_X10);
            break;
        default:
            break;
    }
}

static uint8_t IndustrialTemp_MapSlider(uint16_t x,
                                        uint16_t y,
                                        uint16_t slider_y,
                                        IndustrialTempParam_t param,
                                        int16_t min_value,
                                        int16_t max_value) {
    int32_t mapped;
    uint16_t clamped_x;

    if (y < slider_y || y > (uint16_t)(slider_y + INDUSTRIAL_TEMP_SLIDER_H)) {
        return 0u;
    }

    if (x <= INDUSTRIAL_TEMP_SLIDER_X) {
        clamped_x = INDUSTRIAL_TEMP_SLIDER_X;
    } else if (x >= (uint16_t)(INDUSTRIAL_TEMP_SLIDER_X + INDUSTRIAL_TEMP_SLIDER_W)) {
        clamped_x = INDUSTRIAL_TEMP_SLIDER_X + INDUSTRIAL_TEMP_SLIDER_W;
    } else {
        clamped_x = x;
    }

    mapped = min_value;
    mapped += ((int32_t)(clamped_x - INDUSTRIAL_TEMP_SLIDER_X) * (max_value - min_value)) /
              INDUSTRIAL_TEMP_SLIDER_W;
    IndustrialTemp_SetParam(param, (int16_t)mapped);
    return 1u;
}

uint8_t IndustrialTemp_SetParamFromTouch(uint16_t x, uint16_t y) {
    if (IndustrialTemp_MapSlider(x,
                                 y,
                                 INDUSTRIAL_TEMP_SLIDER_SET_Y,
                                 INDUSTRIAL_TEMP_PARAM_SET_TEMP,
                                 INDUSTRIAL_TEMP_SET_MIN_DC,
                                 INDUSTRIAL_TEMP_SET_MAX_DC)) {
        return 1u;
    }
    if (IndustrialTemp_MapSlider(x,
                                 y,
                                 INDUSTRIAL_TEMP_SLIDER_KP_Y,
                                 INDUSTRIAL_TEMP_PARAM_KP,
                                 INDUSTRIAL_TEMP_KP_MIN_X10,
                                 INDUSTRIAL_TEMP_KP_MAX_X10)) {
        return 1u;
    }
    if (IndustrialTemp_MapSlider(x,
                                 y,
                                 INDUSTRIAL_TEMP_SLIDER_KI_Y,
                                 INDUSTRIAL_TEMP_PARAM_KI,
                                 INDUSTRIAL_TEMP_KI_MIN_X100,
                                 INDUSTRIAL_TEMP_KI_MAX_X100)) {
        return 1u;
    }
    return IndustrialTemp_MapSlider(x,
                                    y,
                                    INDUSTRIAL_TEMP_SLIDER_KD_Y,
                                    INDUSTRIAL_TEMP_PARAM_KD,
                                    INDUSTRIAL_TEMP_KD_MIN_X10,
                                    INDUSTRIAL_TEMP_KD_MAX_X10);
}

void IndustrialTemp_Update(void) {
    int16_t error;
    int16_t derivative;
    int32_t p_term;
    int32_t i_term;
    int32_t d_term;
    int32_t output;
    int32_t heating_dC;
    int32_t cooling_dC;

    error = (int16_t)(g_temp_state.set_temp_dC - g_temp_state.actual_temp_dC);
    g_integral_dC += error;
    if (g_integral_dC > 6000) {
        g_integral_dC = 6000;
    } else if (g_integral_dC < -6000) {
        g_integral_dC = -6000;
    }

    derivative = (int16_t)(error - g_previous_error_dC);
    p_term = ((int32_t)g_temp_state.kp_x10 * error) / 100;
    i_term = ((int32_t)g_temp_state.ki_x100 * g_integral_dC) / 2000;
    d_term = ((int32_t)g_temp_state.kd_x10 * derivative) / 100;
    output = p_term + i_term + d_term;

    g_temp_state.heater_percent = IndustrialTemp_ClampU8(output, 0u, 100u);

    heating_dC = ((int32_t)g_temp_state.heater_percent * 3) / 10;
    cooling_dC = ((int32_t)g_temp_state.actual_temp_dC - INDUSTRIAL_TEMP_AMBIENT_DC) / 35;
    g_temp_state.actual_temp_dC = IndustrialTemp_ClampI16(g_temp_state.actual_temp_dC +
                                                          heating_dC -
                                                          cooling_dC,
                                                          INDUSTRIAL_TEMP_PLOT_MIN_DC,
                                                          INDUSTRIAL_TEMP_PLOT_MAX_DC);
    g_temp_state.error_dC = (int16_t)(g_temp_state.set_temp_dC - g_temp_state.actual_temp_dC);
    g_previous_error_dC = error;
    IndustrialTemp_PushHistory();
}

const IndustrialTempState_t *IndustrialTemp_GetState(void) {
    return &g_temp_state;
}

uint8_t IndustrialTemp_IsInRange(void) {
    int16_t error = g_temp_state.error_dC;

    if (error < 0) {
        error = (int16_t)-error;
    }
    return (error <= INDUSTRIAL_TEMP_IN_RANGE_DC) ? 1u : 0u;
}
```

- [ ] **Step 3: Run model tests**

Run:

```bash
pytest -q tests/test_industrial_temp_model.py
```

Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add User/task/industrial_temp.h User/task/industrial_temp.c tests/test_industrial_temp_model.py
git commit -m "feat: add industrial temperature model"
```

---

### Task 5: Add Failing UI Draw Contracts

**Files:**
- Modify: `tests/test_static_contracts.py`

- [ ] **Step 1: Add UI draw tests**

Append:

```python
def test_industrial_temp_ui_draw_contracts_exist():
    header = read("User/ui/ui_draw.h")
    source = read("User/ui/ui_draw.c")

    assert '#include "industrial_temp.h"' in header
    assert "COLOR_TEMP_SET" in header
    assert "COLOR_TEMP_ACTUAL" in header
    assert "UI_TEMP_CHART_X" in header
    assert "void UI_DrawIndustrialTempView(const IndustrialTempState_t *state);" in header
    assert "void UI_DrawIndustrialTempState(const IndustrialTempState_t *state);" in header

    view_body = body_of(source, "UI_DrawIndustrialTempView")
    state_body = body_of(source, "UI_DrawIndustrialTempState")

    assert "UI_DrawBackButton();" in view_body
    assert "UI_DrawTaskTitle(\"Industrial Temp\");" in view_body
    assert "NT35510_DrawLine" in source
    assert "COLOR_TEMP_SET" in source
    assert "COLOR_TEMP_ACTUAL" in source
    assert "heater_percent" in state_body
    assert "IndustrialTemp_IsInRange()" in state_body
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
pytest -q tests/test_static_contracts.py::test_industrial_temp_ui_draw_contracts_exist
```

Expected: FAIL because draw helpers do not exist.

- [ ] **Step 3: Commit failing test**

```bash
git add tests/test_static_contracts.py
git commit -m "test: add industrial temperature UI draw contracts"
```

---

### Task 6: Implement Chart, Values, And Slider Drawing

**Files:**
- Modify: `User/ui/ui_draw.h`
- Modify: `User/ui/ui_draw.c`

- [ ] **Step 1: Add UI constants and prototypes**

In `User/ui/ui_draw.h`, include the model header and add constants:

```c
#include "industrial_temp.h"

#define COLOR_TEMP_SET      0xFFE0
#define COLOR_TEMP_ACTUAL   0x07FF
#define COLOR_TEMP_GRID     0x39E7
#define COLOR_TEMP_GREEN    0x07E0
#define COLOR_TEMP_RED      0xF800

#define UI_TEMP_CHART_X     50
#define UI_TEMP_CHART_Y     92
#define UI_TEMP_CHART_W     380
#define UI_TEMP_CHART_H     260

#define UI_TEMP_VALUE_X     40
#define UI_TEMP_VALUE_Y     370
#define UI_TEMP_VALUE_W     400
#define UI_TEMP_VALUE_H     48
```

Add prototypes:

```c
void UI_DrawIndustrialTempView(const IndustrialTempState_t *state);
void UI_DrawIndustrialTempState(const IndustrialTempState_t *state);
```

- [ ] **Step 2: Add helpers and public draw functions**

In `User/ui/ui_draw.c`, add helpers after `UI_DrawTaskView()`:

```c
static uint16_t UI_TempToChartY(int16_t temp_dC) {
    int32_t clamped = temp_dC;

    if (clamped < INDUSTRIAL_TEMP_PLOT_MIN_DC) {
        clamped = INDUSTRIAL_TEMP_PLOT_MIN_DC;
    } else if (clamped > INDUSTRIAL_TEMP_PLOT_MAX_DC) {
        clamped = INDUSTRIAL_TEMP_PLOT_MAX_DC;
    }

    return (uint16_t)(UI_TEMP_CHART_Y + UI_TEMP_CHART_H -
                      ((clamped - INDUSTRIAL_TEMP_PLOT_MIN_DC) * UI_TEMP_CHART_H) /
                      (INDUSTRIAL_TEMP_PLOT_MAX_DC - INDUSTRIAL_TEMP_PLOT_MIN_DC));
}

static void UI_DrawIndustrialTempChartFrame(void) {
    uint16_t i;

    UI_FillRect(UI_TEMP_CHART_X - 8,
                UI_TEMP_CHART_Y - 8,
                UI_TEMP_CHART_W + 16,
                UI_TEMP_CHART_H + 16,
                COLOR_BG);
    UI_DrawRect(UI_TEMP_CHART_X, UI_TEMP_CHART_Y, UI_TEMP_CHART_W, UI_TEMP_CHART_H, COLOR_TEXT);

    for (i = 1u; i < 4u; i++) {
        uint16_t y = UI_TEMP_CHART_Y + (UI_TEMP_CHART_H * i) / 4u;
        LCD_SetTextColor(COLOR_TEMP_GRID);
        NT35510_DrawLine(UI_TEMP_CHART_X, y, UI_TEMP_CHART_X + UI_TEMP_CHART_W, y);
    }

    UI_DrawString(UI_TEMP_CHART_X, UI_TEMP_CHART_Y - 28, "Set", COLOR_TEMP_SET, COLOR_BG);
    UI_DrawString(UI_TEMP_CHART_X + 80, UI_TEMP_CHART_Y - 28, "Actual", COLOR_TEMP_ACTUAL, COLOR_BG);
}

static void UI_DrawIndustrialTempTrace(const int16_t *history,
                                       uint16_t count,
                                       uint16_t next_index,
                                       uint16_t color) {
    uint16_t i;
    uint16_t previous_x = 0u;
    uint16_t previous_y = 0u;
    uint16_t first = 1u;
    uint16_t oldest;

    if (history == NULL || count == 0u) {
        return;
    }

    oldest = (count >= INDUSTRIAL_TEMP_HISTORY_LEN) ? next_index : 0u;
    LCD_SetTextColor(color);

    for (i = 0u; i < count; i++) {
        uint16_t index = (uint16_t)((oldest + i) % INDUSTRIAL_TEMP_HISTORY_LEN);
        uint16_t x = (uint16_t)(UI_TEMP_CHART_X + ((uint32_t)i * (UI_TEMP_CHART_W - 1u)) /
                                (INDUSTRIAL_TEMP_HISTORY_LEN - 1u));
        uint16_t y = UI_TempToChartY(history[index]);

        if (first != 0u) {
            first = 0u;
        } else {
            NT35510_DrawLine(previous_x, previous_y, x, y);
        }
        previous_x = x;
        previous_y = y;
    }
}

static void UI_DrawIndustrialTempSlider(uint16_t y,
                                        const char *label,
                                        int16_t value,
                                        int16_t min_value,
                                        int16_t max_value,
                                        uint8_t decimals) {
    char line[32];
    uint16_t fill_w;
    int32_t numerator;

    UI_DrawString(40, y + 6u, label, COLOR_TEXT, COLOR_BG);
    UI_DrawRect(INDUSTRIAL_TEMP_SLIDER_X,
                y + 8u,
                INDUSTRIAL_TEMP_SLIDER_W,
                16u,
                COLOR_BTN_BORDER);

    numerator = value - min_value;
    if (numerator < 0) {
        numerator = 0;
    }
    fill_w = (uint16_t)((numerator * INDUSTRIAL_TEMP_SLIDER_W) / (max_value - min_value));
    if (fill_w > INDUSTRIAL_TEMP_SLIDER_W) {
        fill_w = INDUSTRIAL_TEMP_SLIDER_W;
    }
    UI_FillRect(INDUSTRIAL_TEMP_SLIDER_X, y + 8u, fill_w, 16u, COLOR_SELECTED);

    if (decimals == 2u) {
        snprintf(line, sizeof(line), "%d.%02d", value / 100, value % 100);
    } else {
        snprintf(line, sizeof(line), "%d.%d", value / 10, value % 10);
    }
    UI_DrawStringBounded(INDUSTRIAL_TEMP_SLIDER_X + INDUSTRIAL_TEMP_SLIDER_W + 8u,
                         y,
                         52u,
                         line,
                         COLOR_TEXT,
                         COLOR_BG);
}

void UI_DrawIndustrialTempView(const IndustrialTempState_t *state) {
    UI_FillRect(0, 0, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT, COLOR_BG);
    UI_DrawTaskTitle("Industrial Temp");
    UI_DrawBackButton();
    UI_DrawIndustrialTempChartFrame();
    UI_DrawIndustrialTempState(state);
}

void UI_DrawIndustrialTempState(const IndustrialTempState_t *state) {
    char line[64];
    uint16_t led_color;

    if (state == NULL) {
        return;
    }

    UI_DrawIndustrialTempChartFrame();
    UI_DrawIndustrialTempTrace(state->set_history_dC,
                               state->history_count,
                               state->history_index,
                               COLOR_TEMP_SET);
    UI_DrawIndustrialTempTrace(state->actual_history_dC,
                               state->history_count,
                               state->history_index,
                               COLOR_TEMP_ACTUAL);

    UI_FillRect(UI_TEMP_VALUE_X, UI_TEMP_VALUE_Y, UI_TEMP_VALUE_W, UI_TEMP_VALUE_H, COLOR_BG);
    snprintf(line,
             sizeof(line),
             "Set:%d.%dC Act:%d.%dC",
             state->set_temp_dC / 10,
             state->set_temp_dC % 10,
             state->actual_temp_dC / 10,
             state->actual_temp_dC % 10);
    UI_DrawStringBounded(UI_TEMP_VALUE_X, UI_TEMP_VALUE_Y, UI_TEMP_VALUE_W, line, COLOR_TEXT, COLOR_BG);

    led_color = IndustrialTemp_IsInRange() ? COLOR_TEMP_GREEN : COLOR_TEMP_RED;
    snprintf(line,
             sizeof(line),
             "Err:%d.%dC Heat:%u%%",
             state->error_dC / 10,
             (state->error_dC < 0 ? -state->error_dC : state->error_dC) % 10,
             (unsigned int)state->heater_percent);
    UI_DrawStringBounded(UI_TEMP_VALUE_X, UI_TEMP_VALUE_Y + 24u, UI_TEMP_VALUE_W - 80u, line, led_color, COLOR_BG);

    UI_DrawIndustrialTempSlider(INDUSTRIAL_TEMP_SLIDER_SET_Y,
                                "Set",
                                state->set_temp_dC,
                                INDUSTRIAL_TEMP_SET_MIN_DC,
                                INDUSTRIAL_TEMP_SET_MAX_DC,
                                1u);
    UI_DrawIndustrialTempSlider(INDUSTRIAL_TEMP_SLIDER_KP_Y,
                                "Kp",
                                state->kp_x10,
                                INDUSTRIAL_TEMP_KP_MIN_X10,
                                INDUSTRIAL_TEMP_KP_MAX_X10,
                                1u);
    UI_DrawIndustrialTempSlider(INDUSTRIAL_TEMP_SLIDER_KI_Y,
                                "Ki",
                                state->ki_x100,
                                INDUSTRIAL_TEMP_KI_MIN_X100,
                                INDUSTRIAL_TEMP_KI_MAX_X100,
                                2u);
    UI_DrawIndustrialTempSlider(INDUSTRIAL_TEMP_SLIDER_KD_Y,
                                "Kd",
                                state->kd_x10,
                                INDUSTRIAL_TEMP_KD_MIN_X10,
                                INDUSTRIAL_TEMP_KD_MAX_X10,
                                1u);
}
```

- [ ] **Step 3: Run UI draw contract test**

Run:

```bash
pytest -q tests/test_static_contracts.py::test_industrial_temp_ui_draw_contracts_exist
```

Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add User/ui/ui_draw.h User/ui/ui_draw.c tests/test_static_contracts.py
git commit -m "feat: draw industrial temperature UI"
```

---

### Task 7: Add Failing Touch Routing Contracts

**Files:**
- Modify: `tests/test_static_contracts.py`

- [ ] **Step 1: Add UI task routing test**

Append:

```python
def test_industrial_temp_touch_routing_updates_sliders_and_draws_special_view():
    source = read("User/ui/ui_task.c")
    body = body_of(source, "UI_HandleTouch")

    assert '#include "industrial_temp.h"' in source
    assert "extern const Button_t task_buttons[TASK_BUTTON_COUNT];" in source
    assert "for (int i = 0; i < TASK_BUTTON_COUNT; i++)" in body
    assert "task_buttons[i].task_id == TASK_INDUSTRIAL_TEMP" in body
    assert "UI_DrawIndustrialTempView(IndustrialTemp_GetState());" in body
    assert "IndustrialTemp_SetParamFromTouch(x, y)" in body
    assert "UI_DrawIndustrialTempState(IndustrialTemp_GetState());" in body
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
pytest -q tests/test_static_contracts.py::test_industrial_temp_touch_routing_updates_sliders_and_draws_special_view
```

Expected: FAIL because `ui_task.c` does not route industrial temperature touches yet.

- [ ] **Step 3: Commit failing test**

```bash
git add tests/test_static_contracts.py
git commit -m "test: add industrial temperature touch routing contract"
```

---

### Task 8: Implement Touch Routing And Special Task View Entry

**Files:**
- Modify: `User/ui/ui_task.c`

- [ ] **Step 1: Include industrial model**

Add this include:

```c
#include "industrial_temp.h"
```

- [ ] **Step 2: Reduce debounce for slider interaction**

Add:

```c
#define TOUCH_SLIDER_DEBOUNCE_MS  50
```

Inside `UI_HandleTouch()`, replace the debounce check with:

```c
uint32_t min_interval = (g_current_task == TASK_INDUSTRIAL_TEMP) ?
                        TOUCH_SLIDER_DEBOUNCE_MS :
                        TOUCH_DEBOUNCE_MS;

if (now - last_touch_time < min_interval) {
    return;
}
last_touch_time = now;
```

- [ ] **Step 3: Route touches while industrial task is active**

After the existing back-button check inside `if (g_current_task != TASK_NONE)`, add:

```c
if (g_current_task == TASK_INDUSTRIAL_TEMP) {
    if (IndustrialTemp_SetParamFromTouch(x, y) != 0u) {
        UI_DrawIndustrialTempState(IndustrialTemp_GetState());
    }
}
```

- [ ] **Step 4: Draw the special industrial task view when selected**

Inside the task-button hit block, replace the unconditional `UI_DrawTaskView(&task_buttons[i]);` with:

```c
if (task_buttons[i].task_id == TASK_INDUSTRIAL_TEMP) {
    IndustrialTemp_Reset();
    UI_DrawIndustrialTempView(IndustrialTemp_GetState());
} else {
    UI_DrawTaskView(&task_buttons[i]);
}
```

- [ ] **Step 5: Run touch routing test**

Run:

```bash
pytest -q tests/test_static_contracts.py::test_industrial_temp_touch_routing_updates_sliders_and_draws_special_view
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add User/ui/ui_task.c tests/test_static_contracts.py
git commit -m "feat: route industrial temperature touch sliders"
```

---

### Task 9: Add Failing Worker And Build Contracts

**Files:**
- Modify: `tests/test_static_contracts.py`

- [ ] **Step 1: Add worker and CMake tests**

Append:

```python
def test_industrial_temp_worker_updates_ui_and_leds_without_uart_dependency():
    worker = read("User/task/worker_task.c")
    cmake = read("CMakeLists.txt")

    assert "User/task/industrial_temp.c" in cmake
    assert '#include "industrial_temp.h"' in worker

    cleanup_body = body_of(worker, "Worker_CleanupTask")
    cleanup_case = switch_case_body(cleanup_body, "TASK_INDUSTRIAL_TEMP")
    assert "Worker_AllLedsOff();" in cleanup_case

    worker_body = body_of(worker, "Worker_Task")
    switch_body = switch_case_body(worker_body, "TASK_INDUSTRIAL_TEMP")

    assert "IndustrialTemp_Update();" in switch_body
    assert "UI_DrawIndustrialTempState(IndustrialTemp_GetState());" in switch_body
    assert "IndustrialTemp_IsInRange()" in switch_body
    assert "Worker_SetRgbLed(LED2_PIN);" in switch_body
    assert "Worker_SetRgbLed(LED1_PIN);" in switch_body
    assert "OSTimeDly(200, OS_OPT_TIME_DLY, &os_err);" in switch_body
    assert "g_uart_rx_received" not in switch_body
    assert "printf(" not in switch_body
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
pytest -q tests/test_static_contracts.py::test_industrial_temp_worker_updates_ui_and_leds_without_uart_dependency
```

Expected: FAIL because worker and CMake integration are not implemented.

- [ ] **Step 3: Commit failing test**

```bash
git add tests/test_static_contracts.py
git commit -m "test: add industrial temperature worker contracts"
```

---

### Task 10: Implement Worker Integration And CMake Build

**Files:**
- Modify: `User/task/worker_task.c`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add source to CMake**

In `CMakeLists.txt`, add:

```cmake
    ${CMAKE_SOURCE_DIR}/User/task/industrial_temp.c
```

inside `USER_SOURCES`, next to `worker_task.c`.

- [ ] **Step 2: Include and initialize the model**

In `User/task/worker_task.c`, add:

```c
#include "industrial_temp.h"
```

After peripheral initialization in `Worker_Task()`:

```c
IndustrialTemp_Init();
```

- [ ] **Step 3: Clean up industrial task LEDs**

Add this case to `Worker_CleanupTask()`:

```c
case TASK_INDUSTRIAL_TEMP:
    Worker_AllLedsOff();
    break;
```

- [ ] **Step 4: Reset and draw when entering the industrial task**

In the task-switch block:

```c
} else if (g_current_task == TASK_INDUSTRIAL_TEMP) {
    IndustrialTemp_Reset();
    UI_DrawIndustrialTempView(IndustrialTemp_GetState());
```

- [ ] **Step 5: Add the worker switch case**

Add:

```c
case TASK_INDUSTRIAL_TEMP:
    IndustrialTemp_Update();
    UI_DrawIndustrialTempState(IndustrialTemp_GetState());
    if (IndustrialTemp_IsInRange() != 0u) {
        Worker_SetRgbLed(LED2_PIN);
    } else {
        Worker_SetRgbLed(LED1_PIN);
    }
    OSTimeDly(200, OS_OPT_TIME_DLY, &os_err);
    break;
```

- [ ] **Step 6: Run worker contract test**

Run:

```bash
pytest -q tests/test_static_contracts.py::test_industrial_temp_worker_updates_ui_and_leds_without_uart_dependency
```

Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add User/task/worker_task.c CMakeLists.txt tests/test_static_contracts.py
git commit -m "feat: run industrial temperature worker task"
```

---

### Task 11: Full Verification And Embedded Build

**Files:**
- Verify all modified files.

- [ ] **Step 1: Run all Python tests**

Run:

```bash
pytest -q
```

Expected: PASS.

- [ ] **Step 2: Configure the firmware build**

Run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

Expected: configuration completes without CMake errors.

- [ ] **Step 3: Build firmware**

Run:

```bash
cmake --build build
```

Expected: `zonghe.elf`, `zonghe.hex`, and `zonghe.bin` are generated, and the linker memory usage is printed.

- [ ] **Step 4: Inspect final changed files**

Run:

```bash
git status --short User/app_cfg.h User/task/industrial_temp.h User/task/industrial_temp.c User/task/worker_task.c User/ui/ui_draw.h User/ui/ui_draw.c User/ui/ui_task.c CMakeLists.txt tests/test_static_contracts.py tests/test_industrial_temp_model.py
git diff --stat HEAD -- User/app_cfg.h User/task/industrial_temp.h User/task/industrial_temp.c User/task/worker_task.c User/ui/ui_draw.h User/ui/ui_draw.c User/ui/ui_task.c CMakeLists.txt tests/test_static_contracts.py tests/test_industrial_temp_model.py
```

Expected: only industrial temperature task, UI, worker, CMake, and test files show changes in this scoped check. Existing unrelated workspace changes may still appear in a full `git status`.

- [ ] **Step 5: Final commit for verification fixes if needed**

If the verification steps required small corrections, commit them:

```bash
git add User/app_cfg.h User/task/industrial_temp.h User/task/industrial_temp.c User/task/worker_task.c User/ui/ui_draw.h User/ui/ui_draw.c User/ui/ui_task.c CMakeLists.txt tests/test_static_contracts.py tests/test_industrial_temp_model.py docs/superpowers/plans/2026-05-22-industrial-temperature-simulation.md
git commit -m "fix: verify industrial temperature simulation"
```

---

## Self-Review

- Spec coverage: The plan covers the sixth menu task, PID model, no UART dependency, chart and slider UI, touch routing, red/green LED rule, CMake, and tests.
- Placeholder scan: No deferred requirements or empty implementation steps are present.
- Type consistency: The plan consistently uses `IndustrialTempState_t`, `IndustrialTempParam_t`, `TASK_INDUSTRIAL_TEMP`, `TASK_BUTTON_COUNT`, `IndustrialTemp_SetParamFromTouch()`, `UI_DrawIndustrialTempView()`, and `UI_DrawIndustrialTempState()`.
