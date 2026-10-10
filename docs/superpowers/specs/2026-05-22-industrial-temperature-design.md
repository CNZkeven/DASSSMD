# Industrial Temperature Simulation Design

## Goal

Add a sixth STM32F407 experiment task named "Industrial Temp" that simulates PID temperature control entirely on the board, without using serial communication. The user adjusts the target temperature and PID parameters on the touch LCD, watches the set and actual temperature curves, and uses the RGB LED as an in-range indicator.

## User Experience

The main menu shows six task buttons. The new sixth button is:

- Name: `Industrial Temp`
- Description: `PID Temperature Sim`
- Task ID: `TASK_INDUSTRIAL_TEMP`

When the user enters the task page, the existing back button remains at the top left. The upper half of the 480x800 LCD contains a chart with axes and two temperature traces:

- Yellow trace: set temperature.
- Blue trace: actual simulated temperature.

The lower half contains four touch sliders:

- `Set`: 20C to 120C.
- `Kp`: 0.0 to 10.0.
- `Ki`: 0.00 to 1.00.
- `Kd`: 0.0 to 5.0.

The lower half also shows the current set temperature, actual temperature, error, heating output, and LED state. Touching or dragging a slider updates the matching parameter immediately.

## Architecture

Create a focused module, `User/task/industrial_temp.h` and `User/task/industrial_temp.c`, for all control-domain state and math. This module owns:

- The set temperature, PID gains, actual temperature, PID integral, previous error, heater output, and curve history.
- Parameter range clamping.
- Fixed-point PID calculation.
- Heating and cooling simulation.
- In-range status calculation.

The existing UI and worker layers use the module through a small C API:

- `IndustrialTemp_Init()`
- `IndustrialTemp_Reset()`
- `IndustrialTemp_Update()`
- `IndustrialTemp_SetParamFromTouch()`
- `IndustrialTemp_GetState()`
- `IndustrialTemp_IsInRange()`

`worker_task.c` remains the RTOS execution point. It calls `IndustrialTemp_Update()` every 200 ms when `g_current_task == TASK_INDUSTRIAL_TEMP`, redraws the task data, and controls the RGB LED.

`ui_draw.c` remains responsible for LCD rendering. It adds dedicated drawing helpers for the industrial temperature task:

- Initial task screen with chart frame and slider frame.
- Periodic chart and numeric value refresh.
- Slider fill indicators.

`ui_task.c` remains responsible for touch routing. When the current task is `TASK_INDUSTRIAL_TEMP`, touches outside the back button are passed to the industrial temperature slider hit-test.

## PID And Thermal Model

All calculations use integer fixed-point values to keep firmware size predictable and avoid float `printf` support.

Temperature values are stored in tenths of a degree Celsius:

- `250` means 25.0C.
- `800` means 80.0C.

PID gain values are also stored as scaled integers:

- `Kp`: tenths, 0 to 100.
- `Ki`: hundredths, 0 to 100.
- `Kd`: tenths, 0 to 50.

Each update performs:

1. `error = set_temp - actual_temp`.
2. Clamp the integral accumulator to a bounded range to avoid windup.
3. Compute a heater output from proportional, integral, and derivative terms.
4. Clamp heater output to 0 to 100 percent.
5. Update actual temperature with a first-order thermal model:
   - Heating raises temperature in proportion to heater output.
   - Natural cooling pulls actual temperature toward ambient temperature.

Default values:

- Ambient temperature: 25.0C.
- Initial actual temperature: 25.0C.
- Initial set temperature: 80.0C.
- Initial gains: `Kp=2.0`, `Ki=0.10`, `Kd=0.5`.

## LED Behavior

The task uses the existing RGB LED helper style from `worker_task.c`.

- If `actual_temp` is within `set_temp +/- 5.0C`, turn on the green LED by using `LED2_PIN`.
- Otherwise, turn on the red LED by using `LED1_PIN`.
- `LED3_PIN` remains off for this task.
- When leaving the task, turn off all board LEDs.

This task does not depend on UART receive state, UART interrupts, or serial logging.

## UI Refresh

The worker loop updates the simulation and task display every 200 ms while the task is active. The chart stores a fixed history window that fits the available chart width. On each refresh:

- The chart plotting area is cleared.
- Axes and grid marks are redrawn.
- Set and actual history traces are mapped into the chart area.
- Numeric state and slider fills are refreshed.

The plotting range is fixed at 0C to 130C so the full 20C to 120C setpoint range and overshoot are visible without rescaling.

## Touch Handling

The industrial temperature page keeps the existing back button behavior. Other touches are processed as slider input.

Each slider owns a rectangular touch area. The horizontal touch coordinate maps linearly to the parameter range. Values are clamped to the range and snapped to practical steps:

- `Set`: 1.0C steps.
- `Kp`: 0.1 steps.
- `Ki`: 0.01 steps.
- `Kd`: 0.1 steps.

Touch updates do not block the UI task and do not call UART functions.

## Testing

Add static and behavior-oriented tests in `tests/test_static_contracts.py` before implementation. The tests verify:

- `TASK_INDUSTRIAL_TEMP` exists before `TASK_MAX`.
- `task_buttons` contains six bounded menu buttons.
- The industrial temperature button uses the approved name and description.
- `CMakeLists.txt` builds `User/task/industrial_temp.c`.
- `worker_task.c` handles `TASK_INDUSTRIAL_TEMP`, updates the simulation, refreshes the UI, and controls red/green LEDs using the +/-5C rule.
- `Worker_CleanupTask()` turns LEDs off when leaving the industrial temperature task.
- `ui_task.c` routes touches on the industrial temperature page to the slider handler.
- `ui_draw.h` exposes the industrial temperature draw helpers.
- The implementation avoids `_printf_float`, UART task state, and serial-only communication for the industrial temperature task.

The existing hardware-oriented behavior still needs final validation on the STM32F407 board after the firmware builds.
